#include "PluginProcessor.h"

#include "PluginEditor.h"

TandemProcessor::TandemProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "params", tp::createLayout()) {
    for (int L = 0; L < tp::kLayersExposed; ++L) {
        reader_[L].attach(apvts, L);
        reader_[L].index();
    }
    for (const auto& d : tp::layerDefs()) layerKeys_.push_back(d.key);
    volume_ = apvts.getParameter(tp::kVolumeId);
    volumeRaw_ = apvts.getRawParameterValue(tp::kVolumeId);
    for (auto* q : getParameters()) all_.push_back(dynamic_cast<juce::RangedAudioParameter*>(q));
    presetSnap_ = currentSetting();
    ab_[0] = ab_[1] = presetSnap_;
    abName_[0] = abName_[1] = presetName_;
    engine_.prepare(48000, 512);
}

bool TandemProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TandemProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    engine_.prepare(sampleRate, samplesPerBlock);
    for (int L = 0; L < tp::kLayersExposed; ++L) engine_.layer(L).setParams(reader_[L].read());
    const size_t n = size_t(std::max(samplesPerBlock, 16));
    l_.assign(n, 0.0f);
    r_.assign(n, 0.0f);
    setLatencySamples(juce::roundToInt(engine_.latencySamples()));
}

juce::RangedAudioParameter* TandemProcessor::param(const juce::String& key, int layer) const {
    if (key == tp::kVolumeId) return volume_;
    return apvts.getParameter(tp::layerId(layer, key));
}

float TandemProcessor::value(const juce::String& key, int layer) const {
    if (auto* p = param(key, layer)) return p->convertFrom0to1(p->getValue());
    return 0.0f;
}

void TandemProcessor::set(juce::RangedAudioParameter* p, float v) {
    if (p) p->setValueNotifyingHost(p->convertTo0to1(v));
}

void TandemProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (int(l_.size()) < n) { l_.resize(size_t(n)); r_.resize(size_t(n)); }

    // Settings, once a block.
    for (int L = 0; L < tp::kLayersExposed; ++L) {
        const td::LayerParams p = reader_[L].read();
        engine_.layer(L).setParams(p);
    }
    engine_.setVolume(volumeRaw_->load(std::memory_order_relaxed));
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) engine_.setTempo(float(*bpm));

    // Notes and controllers, each at its own sample.
    int done = 0;
    auto renderTo = [&](int upTo) {
        if (upTo > done) { engine_.render(l_.data() + done, r_.data() + done, upTo - done); done = upTo; }
    };
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        renderTo(juce::jlimit(0, n, meta.samplePosition));
        if (msg.isNoteOn()) engine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) engine_.noteOff(msg.getNoteNumber());
        else if (msg.isPitchWheel()) engine_.bend(float(msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isChannelPressure()) engine_.aftertouch(float(msg.getChannelPressureValue()) / 127.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine_.allNotesOff(msg.isAllSoundOff());
        else if (msg.isController()) {
            const int cc = msg.getControllerNumber();
            const float v = float(msg.getControllerValue()) / 127.0f;
            if (cc == 1) engine_.modWheel(v);
            else if (cc == 11) engine_.expression(v);
            else if (cc == 64) engine_.sustain(v >= 0.5f);
        }
    }
    renderTo(n);
    midi.clear();

    auto* outL = buffer.getWritePointer(0);
    if (buffer.getNumChannels() > 1) {
        std::copy(l_.begin(), l_.begin() + n, outL);
        std::copy(r_.begin(), r_.begin() + n, buffer.getWritePointer(1));
    } else {
        for (int i = 0; i < n; ++i) outL[i] = 0.5f * (l_[size_t(i)] + r_[size_t(i)]);
    }
    odrive_.store(engine_.layer(0).overdriven(), std::memory_order_relaxed);
    sounding_.store(engine_.layer(0).soundingVoices(), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Presets and compare. The volume is not part of a sound.
bool TandemProcessor::isSoundParam(int index) const { return all_[size_t(index)] != volume_; }

TandemProcessor::Setting TandemProcessor::currentSetting() const {
    Setting s(all_.size());
    for (size_t i = 0; i < all_.size(); ++i) s[i] = all_[i]->convertFrom0to1(all_[i]->getValue());
    return s;
}

void TandemProcessor::applySetting(const Setting& s) {
    for (size_t i = 0; i < all_.size() && i < s.size(); ++i)
        if (isSoundParam(int(i))) set(all_[i], s[i]);
}

bool TandemProcessor::presetEdited() const {
    const Setting now = currentSetting();
    for (size_t i = 0; i < now.size() && i < presetSnap_.size(); ++i)
        if (isSoundParam(int(i)) && std::abs(now[i] - presetSnap_[i]) > 1e-4f * (1.0f + std::abs(now[i]))) return true;
    return false;
}

juce::File TandemProcessor::presetFolder() {
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Rumpus Room Records").getChildFile("Tandem").getChildFile("Presets");
}

std::vector<TandemProcessor::PresetEntry> TandemProcessor::presetList() const {
    std::vector<PresetEntry> list;
    for (int i = 0; i < tpre::kNumFactory; ++i) list.push_back({tpre::kFactory[i].name, true, {}});
    auto files = presetFolder().findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    for (const auto& f : files) list.push_back({f.getFileNameWithoutExtension(), false, f});
    return list;
}

void TandemProcessor::loadPreset(int index) {
    const auto list = presetList();
    if (index < 0 || index >= int(list.size())) return;
    Setting s(all_.size());
    for (size_t i = 0; i < all_.size(); ++i) s[i] = all_[i]->convertFrom0to1(all_[i]->getDefaultValue());
    auto slot = [&](juce::RangedAudioParameter* q) -> float* {
        const auto it = std::find(all_.begin(), all_.end(), q);
        return it == all_.end() ? nullptr : &s[size_t(it - all_.begin())];
    };
    if (list[size_t(index)].factory) {
        for (const auto& kv : tpre::kFactory[index].s) {
            if (kv.key == nullptr) break;
            if (auto* v = slot(param(kv.key))) *v = kv.v;
            else jassertfalse;
        }
    } else {
        auto xml = juce::XmlDocument::parse(list[size_t(index)].file);
        if (!xml || !xml->hasTagName("TandemPreset")) return;
        for (auto* e : xml->getChildWithTagNameIterator("p"))
            if (auto* v = slot(apvts.getParameter(e->getStringAttribute("id")))) *v = float(e->getDoubleAttribute("v"));
    }
    applySetting(s);
    presetIndex_ = index;
    presetName_ = list[size_t(index)].name;
    presetSnap_ = currentSetting();
}

void TandemProcessor::stepPreset(int delta) {
    const int n = int(presetList().size());
    if (n > 0) loadPreset(((presetIndex_ + delta) % n + n) % n);
}

juce::String TandemProcessor::saveUserPreset(const juce::String& rawName) {
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) return "Give the preset a name.";
    for (int i = 0; i < tpre::kNumFactory; ++i)
        if (name.equalsIgnoreCase(tpre::kFactory[i].name)) return "That name belongs to a factory preset.";
    const auto folder = presetFolder();
    if (!folder.createDirectory()) return "Couldn't create " + folder.getFullPathName();
    juce::XmlElement xml("TandemPreset");
    xml.setAttribute("name", name);
    const Setting s = currentSetting();
    for (size_t i = 0; i < all_.size(); ++i) {
        const float def = all_[i]->convertFrom0to1(all_[i]->getDefaultValue());
        if (!isSoundParam(int(i)) || std::abs(s[i] - def) < 1e-6f) continue;
        auto* e = xml.createNewChildElement("p");
        e->setAttribute("id", all_[i]->getParameterID());
        e->setAttribute("v", double(s[i]));
    }
    const auto file = folder.getChildFile(name + ".xml");
    if (!xml.writeTo(file)) return "Couldn't write " + file.getFullPathName();
    const auto list = presetList();
    for (int i = 0; i < int(list.size()); ++i)
        if (!list[size_t(i)].factory && list[size_t(i)].file == file) presetIndex_ = i;
    presetName_ = name;
    presetSnap_ = s;
    return {};
}

bool TandemProcessor::deleteUserPreset(int index) {
    const auto list = presetList();
    if (index < 0 || index >= int(list.size()) || list[size_t(index)].factory) return false;
    const bool ok = list[size_t(index)].file.deleteFile();
    if (ok && index == presetIndex_) { presetIndex_ = 0; presetName_ << " (deleted)"; }
    return ok;
}

void TandemProcessor::selectAB(int slot) {
    if (slot == abSlot_) return;
    ab_[abSlot_] = currentSetting(); abName_[abSlot_] = presetName_; abIndex_[abSlot_] = presetIndex_;
    abSlot_ = slot;
    applySetting(ab_[slot]);
    presetName_ = abName_[slot]; presetIndex_ = abIndex_[slot];
}

juce::AudioProcessorEditor* TandemProcessor::createEditor() { return new TandemEditor(*this); }

void TandemProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("uiScale", uiScale, nullptr);
    state.setProperty("menu", menu, nullptr);
    for (int i = 0; i < 3; ++i) state.setProperty("page" + juce::String(i), page[i], nullptr);
    state.setProperty("presetName", presetName_, nullptr);
    state.setProperty("presetIndex", presetIndex_, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void TandemProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid() || !state.hasType(apvts.state.getType())) return;
        uiScale = float(state.getProperty("uiScale", 0.8f));
        menu = juce::jlimit(0, 2, int(state.getProperty("menu", 1)));
        for (int i = 0; i < 3; ++i) page[i] = int(state.getProperty("page" + juce::String(i), 0));
        presetName_ = state.getProperty("presetName", "init").toString();
        presetIndex_ = int(state.getProperty("presetIndex", 0));
        apvts.replaceState(state);
        presetSnap_ = currentSetting();
        ab_[0] = ab_[1] = presetSnap_; abName_[0] = abName_[1] = presetName_; abSlot_ = 0;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TandemProcessor(); }
