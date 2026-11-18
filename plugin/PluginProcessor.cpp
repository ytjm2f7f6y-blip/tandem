#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>

juce::AudioProcessor::BusesProperties TandemProcessor::buses() {
    auto b = BusesProperties();
   #if TD_SIDECHAIN && TD_SIDECHAIN_ON
    b = b.withInput("Side Chain", juce::AudioChannelSet::stereo(), true);      // the EXT IN: on from the start (Tandem SC)
   #elif TD_SIDECHAIN
    b = b.withInput("Side Chain", juce::AudioChannelSet::stereo(), false);     // the EXT IN: off until the host routes one in
   #endif
    return b.withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

TandemProcessor::TandemProcessor()
    : AudioProcessor(buses()),
      apvts(*this, nullptr, "params", tp::createLayout()) {
    for (int L = 0; L < tp::kLayersExposed; ++L) {
        reader_[L].attach(apvts, L);
        reader_[L].index();
    }
    perfReader_.attach(apvts);
    editLayerRaw_ = apvts.getRawParameterValue(tp::kEditLayerId);
    for (const auto& d : tp::layerDefs()) layerKeys_.push_back(d.key);
    volume_ = apvts.getParameter(tp::kVolumeId);
    volumeRaw_ = apvts.getRawParameterValue(tp::kVolumeId);
    setup_.tune = apvts.getRawParameterValue("setup_tune");
    setup_.transpose = apvts.getRawParameterValue("setup_transpose");
    setup_.velocity = apvts.getRawParameterValue("setup_velocity");
    setup_.pressure = apvts.getRawParameterValue("setup_pressure");
    setup_.ch1 = apvts.getRawParameterValue("setup_ch1");
    setup_.ch2 = apvts.getRawParameterValue("setup_ch2");
    setup_.age = apvts.getRawParameterValue("setup_age");
    setup_.pedal = apvts.getRawParameterValue("setup_pedal");
    std::fill(std::begin(noteMap_), std::end(noteMap_), -1);
    for (auto* q : getParameters()) all_.push_back(dynamic_cast<juce::RangedAudioParameter*>(q));
    presetSnap_ = currentSetting();
    ab_[0] = ab_[1] = presetSnap_;
    abName_[0] = abName_[1] = presetName_;
    for (int L = 0; L < 2; ++L) layerSnap_[size_t(L)] = layerValues(L);
    abLayers_[0] = abLayers_[1] = layerNames();
    engine_.prepare(48000, 512);
}

bool TandemProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (layouts.inputBuses.isEmpty()) return true;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void TandemProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    engine_.prepare(sampleRate, samplesPerBlock);
    engine_.setPerf(perfReader_.read());
    for (int L = 0; L < tp::kLayersExposed; ++L) engine_.layer(L).setParams(reader_[L].read());
    const size_t n = size_t(std::max(samplesPerBlock, 16));
    l_.assign(n, 0.0f);
    r_.assign(n, 0.0f);
    scL_.assign(n, 0.0f);
    scR_.assign(n, 0.0f);
    setLatencySamples(juce::roundToInt(engine_.latencySamples()));
}

juce::RangedAudioParameter* TandemProcessor::param(const juce::String& key, int layer) const {
    if (key.length() > 2 && key[1] == ':' && (key[0] == '1' || key[0] == '2')) return param(key.substring(2), key[0] - '1');
    if (key == tp::kVolumeId) return volume_;
    if (tp::isSetupId(key) || tp::isPerfId(key)) return apvts.getParameter(key);
    return apvts.getParameter(tp::layerId(layer < 0 ? editLayer() : juce::jlimit(0, 1, layer), key));
}

void TandemProcessor::setEditLayer(int layer) {
    if (auto* p = apvts.getParameter(tp::kEditLayerId)) {
        p->beginChangeGesture();
        set(p, float(juce::jlimit(0, 1, layer)));
        p->endChangeGesture();
    }
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

    // The side chain (the EXT IN), before the output overwrites the channels it shares.
    const bool side = getBusCount(true) > 0 && getBus(true, 0)->isEnabled() && getBus(true, 0)->getNumberOfChannels() > 0;
    if (int(scL_.size()) < n) { scL_.resize(size_t(n)); scR_.resize(size_t(n)); }
    if (side) {
        auto in = getBusBuffer(buffer, true, 0);
        std::copy(in.getReadPointer(0), in.getReadPointer(0) + n, scL_.begin());
        std::copy(in.getReadPointer(in.getNumChannels() > 1 ? 1 : 0), in.getReadPointer(in.getNumChannels() > 1 ? 1 : 0) + n, scR_.begin());
    }
    sideOn_.store(side, std::memory_order_relaxed);
    engine_.setExtConnected(side);

    // Settings, once a block. AGE scales every patch's drift (0.5 leaves it as made).
    auto rd = [](std::atomic<float>* a) { return a->load(std::memory_order_relaxed); };
    const float age = rd(setup_.age);
    const td::PerfParams perf = perfReader_.read();
    engine_.setPerf(perf);
    for (int L = 0; L < tp::kLayersExposed; ++L) {
        td::LayerParams p = reader_[L].read();
        p.drift = juce::jlimit(0.0f, 1.0f, p.drift * (0.25f + 1.5f * age) + std::max(0.0f, age - 0.75f) * 0.4f);
        engine_.layer(L).setParams(p);
    }
    engine_.setVolume(volumeRaw_->load(std::memory_order_relaxed));
    engine_.setTune(rd(setup_.tune) / 100.0f);
    const int transpose = juce::roundToInt(rd(setup_.transpose));
    const int velCurve = juce::roundToInt(rd(setup_.velocity)), pressCurve = juce::roundToInt(rd(setup_.pressure));
    const int ch1 = perf.chan[0], ch2 = perf.chan[1];
    const bool pedalInv = rd(setup_.pedal) >= 0.5f;
    if (pedalInv != lastPedalInv_) {          // turned round: read the pedal where it is now
        lastPedalInv_ = pedalInv;
        engine_.sustain((lastPedal_ >= 0.5f) != pedalInv);
    }
    {   // the host's transport: its tempo, and its beat while it plays
        bool playing = false;
        double ppq = 0.0, bpm = 0.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition()) {
                if (auto b = pos->getBpm()) bpm = *b;
                if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
            }
        engine_.setTransport(playing, ppq, bpm);
    }
    const bool learning = learning_.load(std::memory_order_relaxed);

    // Notes and controllers, each at its own sample.
    int done = 0;
    auto renderTo = [&](int upTo) {
        if (upTo > done) {
            engine_.render(l_.data() + done, r_.data() + done, upTo - done, side ? scL_.data() + done : nullptr, side ? scR_.data() + done : nullptr);
            done = upTo;
        }
    };
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        renderTo(juce::jlimit(0, n, meta.samplePosition));
        const int ch = msg.getChannel();
        // A channel neither layer listens on is ignored. (Notes: the engine gives each layer its own.)
        if (ch > 0 && ch1 > 0 && ch2 > 0 && ch != ch1 && ch != ch2) continue;
        if (msg.isNoteOn()) {
            const int key = msg.getNoteNumber();
            const int note = juce::jlimit(0, 127, key + transpose);
            float v = msg.getFloatVelocity();
            if (velCurve == 0) v = std::pow(v, 0.6f);
            else if (velCurve == 2) v = std::pow(v, 1.7f);
            const int slot = (ch > 0 ? (ch - 1) & 15 : 0) * 128 + key;
            if (noteMap_[slot] >= 0) engine_.noteOff(noteMap_[slot], ch);
            noteMap_[slot] = note;
            if (learning) { learnNote_.store(note); learning_.store(false); }
            engine_.noteOn(note, v, ch);
        } else if (msg.isNoteOff()) {
            const int key = msg.getNoteNumber();
            const int slot = (ch > 0 ? (ch - 1) & 15 : 0) * 128 + key;
            engine_.noteOff(noteMap_[slot] >= 0 ? noteMap_[slot] : juce::jlimit(0, 127, key + transpose), ch);
            noteMap_[slot] = -1;
        } else if (msg.isPitchWheel()) engine_.bend(float(msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isChannelPressure()) {
            float v = float(msg.getChannelPressureValue()) / 127.0f;
            v = pressCurve == 0 ? 0.0f : (pressCurve == 2 ? v * v : v);
            engine_.aftertouch(v);
        } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
            engine_.allNotesOff(msg.isAllSoundOff());
            std::fill(std::begin(noteMap_), std::end(noteMap_), -1);
        } else if (msg.isController()) {
            const int cc = msg.getControllerNumber();
            const float v = float(msg.getControllerValue()) / 127.0f;
            if (cc == 1) engine_.modWheel(v);
            else if (cc == 11) engine_.expression(v);
            else if (cc == 64) { lastPedal_ = v; engine_.sustain((v >= 0.5f) != pedalInv); }
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
    const int ed = juce::jlimit(0, 1, perf.layer);
    odrive_.store(engine_.layer(ed).overdriven(), std::memory_order_relaxed);
    sounding_.store(engine_.layer(0).soundingVoices() + engine_.layer(1).soundingVoices(), std::memory_order_relaxed);
    beat_.store(engine_.beat(), std::memory_order_relaxed);
    tempo_.store(engine_.tempo(), std::memory_order_relaxed);
    follow_.store(engine_.follower(), std::memory_order_relaxed);
    for (int L = 0; L < 2; ++L) arpRun_[size_t(L)].store(engine_.arp(L).running(), std::memory_order_relaxed);
    for (int L = 0; L < tp::kLayersExposed; ++L) {
        const auto& layer = engine_.layer(L);
        const int per = layer.binaural() ? 2 : 1;           // the stereo routing: a voice is two of the panel's
        vcount_[size_t(L)].store(std::min(kMeterVoices, layer.voiceCount() * per), std::memory_order_relaxed);
        for (int v = 0; v < kMeterVoices; ++v) meters_[size_t(L)][size_t(v)].store(layer.voiceMeter(v / per), std::memory_order_relaxed);
    }
    for (int d = 0; d < td::kNumModDests; ++d) modNow_[size_t(d)].store(engine_.layer(ed).modNow(d), std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Presets and compare. The volume and the setup drawer are not part of a sound.
bool TandemProcessor::isSoundParam(int index) const {
    auto* p = all_[size_t(index)];
    return p != volume_ && !tp::isSetupId(p->getParameterID());
}

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
        if (isSoundParam(int(i)) && all_[i]->getParameterID() != tp::kEditLayerId     // looking at the other layer is not an edit
            && std::abs(now[i] - presetSnap_[i]) > 1e-4f * (1.0f + std::abs(now[i]))) return true;
    return false;
}

juce::File TandemProcessor::presetFolder() {
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Rumpus Room Records").getChildFile("Tandem").getChildFile("Presets");
}

std::vector<TandemProcessor::PresetEntry> TandemProcessor::presetList() const {
    std::vector<PresetEntry> list;
    for (int i = 0; i < tpre::kNumFactory; ++i) {
        const juce::String cat(tpre::kFactory[i].category);
        list.push_back({tpre::kFactory[i].name, true, {}, cat, cat == "Both layers"});
    }
    auto files = presetFolder().findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    for (const auto& f : files) {
        bool layer = false;                              // a patch saved from one layer; anything else is a performance
        if (auto in = f.createInputStream()) layer = in->readString().contains("kind=\"layer\"");
        list.push_back({f.getFileNameWithoutExtension(), false, f, layer ? "Yours" : "Your performances", !layer});
    }
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
            if (auto* v = slot(param(kv.key, 0))) *v = kv.v;          // a plain key is layer 1's; "2:key" is layer 2's
            else jassertfalse;
        }
    } else {
        auto xml = juce::XmlDocument::parse(list[size_t(index)].file);
        if (!xml || !xml->hasTagName("TandemPreset")) return;
        const auto map = readWaves(*xml);
        for (auto* e : xml->getChildWithTagNameIterator("p")) {
            const auto id = e->getStringAttribute("id");
            if (auto* v = slot(apvts.getParameter(id))) *v = id.contains("_alt") ? mapWave(float(e->getDoubleAttribute("v")), map) : float(e->getDoubleAttribute("v"));
        }
    }
    applySetting(s);
    presetIndex_ = index;
    presetName_ = list[size_t(index)].name;
    presetSnap_ = currentSetting();
    const bool both = list[size_t(index)].bothLayers;     // the layers' windows: a performance on both, a patch on layer 1
    for (int L = 0; L < 2; ++L) {
        layerName_[size_t(L)] = both ? list[size_t(index)].name + " (" + juce::String(L + 1) + ")" : (L == 0 ? list[size_t(index)].name : juce::String("init"));
        layerIndex_[size_t(L)] = both || L == 0 ? index : 0;
        layerPart_[size_t(L)] = both ? L : 0;
        layerSnap_[size_t(L)] = layerValues(L);
    }
}

// ---------------------------------------------------------------------------
// Each layer's sound
TandemProcessor::Setting TandemProcessor::layerValues(int layer) const {
    Setting v;
    for (const auto& k : layerKeys_) if (auto* q = param(k, layer)) v.push_back(q->convertFrom0to1(q->getValue()));
    return v;
}

bool TandemProcessor::layerEdited(int layer) const {
    const auto now = layerValues(layer);
    const auto& was = layerSnap_[size_t(layer & 1)];
    if (was.size() != now.size()) return false;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::abs(now[i] - was[i]) > 1e-4f * (1.0f + std::abs(now[i]))) return true;
    return false;
}

std::vector<int> TandemProcessor::layerPatches() const {
    std::vector<int> v;
    const auto list = presetList();
    for (int i = 0; i < int(list.size()); ++i) if (!list[size_t(i)].bothLayers) v.push_back(i);
    return v;
}

void TandemProcessor::nameWhole() {
    const auto strip = [](juce::String n) { return n.endsWith(")") && n.contains(" (") ? n.upToLastOccurrenceOf(" (", false, false) : n; };
    const auto a = strip(layerName_[0]), b = strip(layerName_[1]);
    presetName_ = a == b ? a : (b == "init" ? a : a + " + " + b);
    presetIndex_ = -1;
}

void TandemProcessor::loadIntoLayer(int index, int layer, int part) {
    const auto list = presetList();
    if (index < 0 || index >= int(list.size())) return;
    layer = juce::jlimit(0, 1, layer);
    part = juce::jlimit(0, 1, part);
    Setting s = currentSetting();
    auto slot = [&](juce::RangedAudioParameter* q) -> float* {
        const auto it = std::find(all_.begin(), all_.end(), q);
        return it == all_.end() ? nullptr : &s[size_t(it - all_.begin())];
    };
    for (const auto& k : layerKeys_)                      // the layer starts from its defaults
        if (auto* q = param(k, layer)) if (auto* v = slot(q)) *v = q->convertFrom0to1(q->getDefaultValue());
    const auto& e = list[size_t(index)];
    if (e.factory) {
        for (const auto& kv : tpre::kFactory[index].s) {
            if (kv.key == nullptr) break;
            juce::String key(kv.key);
            int from = 0;
            if (key.startsWith("2:")) { from = 1; key = key.substring(2); }
            if (tp::isPerfId(key) || tp::isSetupId(key) || key == tp::kVolumeId || from != part) continue;
            if (auto* v = slot(param(key, layer))) *v = kv.v;
        }
    } else {
        auto xml = juce::XmlDocument::parse(e.file);
        if (!xml || !xml->hasTagName("TandemPreset")) return;
        const auto map = readWaves(*xml, 1 - layer);          // the other layer's waves stay as they are
        const juce::String pre = "l" + juce::String(part + 1) + "_";
        for (auto* p : xml->getChildWithTagNameIterator("p")) {
            const auto id = p->getStringAttribute("id");
            if (id.startsWith(pre))
                if (auto* v = slot(param(id.substring(pre.length()), layer)))
                    *v = id.contains("_alt") ? mapWave(float(p->getDoubleAttribute("v")), map) : float(p->getDoubleAttribute("v"));
        }
    }
    applySetting(s);
    layerName_[size_t(layer)] = e.bothLayers ? e.name + " (" + juce::String(part + 1) + ")" : e.name;
    layerIndex_[size_t(layer)] = index;
    layerPart_[size_t(layer)] = part;
    layerSnap_[size_t(layer)] = layerValues(layer);
    nameWhole();
    presetSnap_ = currentSetting();
}

void TandemProcessor::stepLayer(int layer, int delta) {
    const auto v = layerPatches();
    if (v.empty()) return;
    const int n = int(v.size());
    int at = -1;
    for (int i = 0; i < n; ++i) if (v[size_t(i)] == layerIndex_[size_t(layer & 1)]) at = i;
    const int next = at < 0 ? (delta > 0 ? 0 : n - 1) : ((at + delta) % n + n) % n;
    loadIntoLayer(v[size_t(next)], layer, 0);
}

juce::String TandemProcessor::saveLayerPatch(const juce::String& rawName, int layer) {
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty()) return "Give the patch a name.";
    for (int i = 0; i < tpre::kNumFactory; ++i)
        if (name.equalsIgnoreCase(tpre::kFactory[i].name)) return "That name belongs to a factory sound.";
    const auto folder = presetFolder();
    if (!folder.createDirectory()) return "Couldn't create " + folder.getFullPathName();
    juce::XmlElement xml("TandemPreset");
    xml.setAttribute("name", name);
    xml.setAttribute("kind", "layer");
    writeWaves(xml, true, layer);
    for (const auto& k : layerKeys_) {
        auto* q = param(k, layer);
        if (!q) continue;
        const float v = q->convertFrom0to1(q->getValue()), def = q->convertFrom0to1(q->getDefaultValue());
        if (std::abs(v - def) < 1e-6f) continue;
        auto* e = xml.createNewChildElement("p");
        e->setAttribute("id", tp::layerId(0, k));         // stored as layer 1's: it goes on either
        e->setAttribute("v", double(v));
    }
    const auto file = folder.getChildFile(name + ".xml");
    if (!xml.writeTo(file)) return "Couldn't write " + file.getFullPathName();
    const auto list = presetList();
    for (int i = 0; i < int(list.size()); ++i)
        if (!list[size_t(i)].factory && list[size_t(i)].file == file) layerIndex_[size_t(layer & 1)] = i;
    layerName_[size_t(layer & 1)] = name;
    layerPart_[size_t(layer & 1)] = 0;
    layerSnap_[size_t(layer & 1)] = layerValues(layer);
    nameWhole();
    return {};
}

TandemProcessor::LayerNames TandemProcessor::layerNames() const {
    LayerNames n;
    n.name = layerName_; n.index = layerIndex_; n.part = layerPart_; n.snap = layerSnap_;
    return n;
}

void TandemProcessor::setLayerNames(const LayerNames& n) {
    layerName_ = n.name; layerIndex_ = n.index; layerPart_ = n.part; layerSnap_ = n.snap;
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
    xml.setAttribute("kind", "performance");
    writeWaves(xml, true);
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
    ab_[abSlot_] = currentSetting(); abName_[abSlot_] = presetName_; abIndex_[abSlot_] = presetIndex_; abLayers_[abSlot_] = layerNames();
    abSlot_ = slot;
    applySetting(ab_[slot]);
    presetName_ = abName_[slot]; presetIndex_ = abIndex_[slot]; setLayerNames(abLayers_[slot]);
}

// ---------------------------------------------------------------------------
// Undo and redo
void TandemProcessor::beginEdit(const juce::String& what) {
    if (editing_) return;
    pending_ = snapshot();
    pendingWhat_ = what;
    editing_ = true;
}

void TandemProcessor::endEdit() {
    if (!editing_) return;
    editing_ = false;
    Snapshot now = snapshot();
    if (now.values == pending_.values && now.index == pending_.index) return;
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (!undo_.empty() && redo_.empty() && undo_.back().what == pendingWhat_ && t - undo_.back().when < 1.0) {
        undo_.back().after = std::move(now);         // the same control again, straight away: one step
        undo_.back().when = t;
        return;
    }
    undo_.push_back({pendingWhat_, std::move(pending_), std::move(now), t});
    if (undo_.size() > 200) undo_.erase(undo_.begin());
    redo_.clear();
}

void TandemProcessor::restore(const Snapshot& s) {
    applySetting(s.values);
    presetSnap_ = s.snap;
    presetName_ = s.name;
    presetIndex_ = s.index;
    setLayerNames(s.layers);
}

bool TandemProcessor::undo() {
    editing_ = false;
    if (undo_.empty()) return false;
    UndoStep step = std::move(undo_.back());
    undo_.pop_back();
    restore(step.before);
    step.when = 0;
    redo_.push_back(std::move(step));
    return true;
}

bool TandemProcessor::redo() {
    editing_ = false;
    if (redo_.empty()) return false;
    UndoStep step = std::move(redo_.back());
    redo_.pop_back();
    restore(step.after);
    undo_.push_back(std::move(step));
    return true;
}

// ---------------------------------------------------------------------------
// Your own waves
juce::String TandemProcessor::importWave(int slot, const juce::File& file) {
    if (slot < 0 || slot >= td::kUserWaves) return "No such slot.";
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
    if (!r) return "Tandem can't read " + file.getFileName() + " (WAV, AIFF or FLAC, please).";
    const auto n = int(r->lengthInSamples);
    if (n < 16) return file.getFileName() + " is too short to be a wave.";
    // One cycle: a file of whole 2048-point frames (a wavetable) gives its first; anything up to 8192 points is one cycle.
    int len = n;
    if (n > 8192) {
        if (n % 2048 == 0) len = 2048;
        else return file.getFileName() + " is longer than one cycle. Tandem takes single-cycle waves (up to 8192 points, or a wavetable of 2048-point frames).";
    }
    juce::AudioBuffer<float> buf(int(r->numChannels), len);
    r->read(&buf, 0, len, 0, true, true);
    std::vector<float> cyc(kCycle);
    const float* x = buf.getReadPointer(0);
    for (int i = 0; i < kCycle; ++i) {                      // to 2048 points, round the loop
        const double pos = double(i) * double(len) / double(kCycle);
        const int i0 = int(pos);
        const double f = pos - i0;
        cyc[size_t(i)] = float(x[i0 % len] * (1.0 - f) + x[(i0 + 1) % len] * f);
    }
    double mean = 0;
    for (float v : cyc) mean += v;
    mean /= kCycle;
    float peak = 0;
    for (auto& v : cyc) { v -= float(mean); peak = std::max(peak, std::abs(v)); }
    if (peak < 1e-5f) return file.getFileName() + " is silent.";
    for (auto& v : cyc) v /= peak;
    setUserWave(slot, file.getFileNameWithoutExtension().substring(0, 24), cyc);
    return {};
}

void TandemProcessor::setUserWave(int slot, const juce::String& name, const std::vector<float>& cycle) {
    if (slot < 0 || slot >= td::kUserWaves || int(cycle.size()) != kCycle) return;
    auto t = std::make_unique<td::WaveTable>();
    std::vector<double> d(cycle.begin(), cycle.end());
    t->fromCycle(d, 1.0f, 512);                             // band-limited, as the built-in alternatives
    t->name = name.toStdString();
    engine_.userWaves().t[size_t(slot)].store(t.get(), std::memory_order_release);
    if (userTable_[size_t(slot)]) retired_.push_back(std::move(userTable_[size_t(slot)]));
    if (retired_.size() > 24) retired_.erase(retired_.begin(), retired_.begin() + 8);
    userTable_[size_t(slot)] = std::move(t);
    userName_[size_t(slot)] = name;
    userCycle_[size_t(slot)] = cycle;
}

void TandemProcessor::clearUserWave(int slot) {
    if (slot < 0 || slot >= td::kUserWaves) return;
    engine_.userWaves().t[size_t(slot)].store(nullptr, std::memory_order_release);
    if (userTable_[size_t(slot)]) retired_.push_back(std::move(userTable_[size_t(slot)]));
    userName_[size_t(slot)] = {};
    userCycle_[size_t(slot)].clear();
}

int TandemProcessor::freeUserWave() const {
    for (int k = 0; k < td::kUserWaves; ++k) if (userName_[size_t(k)].isEmpty()) return k;
    return -1;
}

bool TandemProcessor::waveUsed(int slot, int layer) const {
    const int idx = td::WaveBank::kNumAlt + slot;
    for (int L = 0; L < 2; ++L) {
        if (layer >= 0 && layer != L) continue;
        for (const char* k : {"osc1_alt1", "osc1_alt2", "osc1_alt3", "osc1_alt4", "osc1_alt5", "osc1_alt6", "osc2_alt1", "osc2_alt2", "osc2_alt3", "lfo_alt1", "lfo_alt2"})
            if (juce::roundToInt(value(k, L)) == idx) return true;
    }
    return false;
}

void TandemProcessor::writeWaves(juce::XmlElement& into, bool onlyUsed, int layer) const {
    for (int k = 0; k < td::kUserWaves; ++k) {
        if (userCycle_[size_t(k)].empty() || (onlyUsed && !waveUsed(k, layer))) continue;
        juce::MemoryBlock mb;
        for (float v : userCycle_[size_t(k)]) {
            const auto s16 = int16_t(juce::jlimit(-32767, 32767, juce::roundToInt(v * 32767.0f)));
            mb.append(&s16, sizeof s16);
        }
        auto* e = into.createNewChildElement("wave");
        e->setAttribute("slot", k);
        e->setAttribute("name", userName_[size_t(k)]);
        e->setAttribute("data", mb.toBase64Encoding());
    }
}

std::array<int, td::kUserWaves> TandemProcessor::readWaves(const juce::XmlElement& from, int keepLayer) {
    std::array<int, td::kUserWaves> map;
    for (int k = 0; k < td::kUserWaves; ++k) map[size_t(k)] = k;
    std::array<bool, td::kUserWaves> taken{};               // given to a wave from this file already
    auto same = [&](int j, const juce::String& name, const std::vector<float>& c) {
        const auto& h = userCycle_[size_t(j)];
        if (h.size() != c.size() || userName_[size_t(j)] != name) return false;
        for (size_t i = 0; i < c.size(); ++i) if (std::abs(h[i] - c[i]) > 1.0f / 8000.0f) return false;
        return true;
    };
    auto keep = [&](int j) { return keepLayer >= 0 && !userCycle_[size_t(j)].empty() && waveUsed(j, keepLayer); };
    for (auto* e : from.getChildWithTagNameIterator("wave")) {
        const int k = e->getIntAttribute("slot", -1);
        juce::MemoryBlock mb;
        if (k < 0 || k >= td::kUserWaves || !mb.fromBase64Encoding(e->getStringAttribute("data")) || mb.getSize() != size_t(kCycle) * 2) continue;
        std::vector<float> cyc(kCycle);
        const auto* d = static_cast<const int16_t*>(mb.getData());
        for (int i = 0; i < kCycle; ++i) cyc[size_t(i)] = float(d[i]) / 32767.0f;
        const auto name = e->getStringAttribute("name");
        int to = -1;
        for (int j = 0; j < td::kUserWaves && to < 0; ++j) if (same(j, name, cyc)) to = j;     // already here: shared
        if (to < 0 && !taken[size_t(k)] && !keep(k)) to = k;                                    // its own slot
        for (int j = 0; j < td::kUserWaves && to < 0; ++j)                                      // an empty one
            if (!taken[size_t(j)] && userCycle_[size_t(j)].empty()) to = j;
        for (int j = 0; j < td::kUserWaves && to < 0; ++j)                                      // one nothing kept uses
            if (!taken[size_t(j)] && !keep(j)) to = j;
        map[size_t(k)] = to;
        if (to < 0) continue;
        taken[size_t(to)] = true;
        if (!same(to, name, cyc)) setUserWave(to, name, cyc);
    }
    return map;
}

float TandemProcessor::mapWave(float v, const std::array<int, td::kUserWaves>& map) {
    const int i = juce::roundToInt(v) - td::WaveBank::kNumAlt;
    if (i < 0 || i >= td::kUserWaves) return v;
    return map[size_t(i)] < 0 ? 0.0f : float(td::WaveBank::kNumAlt + map[size_t(i)]);
}

juce::AudioProcessorEditor* TandemProcessor::createEditor() { return new TandemEditor(*this); }

void TandemProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("uiScale", uiScale, nullptr);
    state.setProperty("menu", menu, nullptr);
    for (int i = 0; i < kMenus; ++i) state.setProperty("page" + juce::String(i), page[i], nullptr);
    state.setProperty("bank", bank, nullptr);
    state.setProperty("drawer", drawerOpen, nullptr);
    state.setProperty("presetName", presetName_, nullptr);
    state.setProperty("presetIndex", presetIndex_, nullptr);
    for (int L = 0; L < 2; ++L) {
        state.setProperty("layerName" + juce::String(L), layerName_[size_t(L)], nullptr);
        state.setProperty("layerIndex" + juce::String(L), layerIndex_[size_t(L)], nullptr);
        state.setProperty("layerPart" + juce::String(L), layerPart_[size_t(L)], nullptr);
    }
    if (auto xml = state.createXml()) {
        auto waves = std::make_unique<juce::XmlElement>("UserWaves");
        writeWaves(*waves, false);
        xml->addChildElement(waves.release());
        copyXmlToBinary(*xml, destData);
    }
}

void TandemProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        for (int k = 0; k < td::kUserWaves; ++k) clearUserWave(k);
        if (auto* w = xml->getChildByName("UserWaves")) { readWaves(*w); xml->removeChildElement(w, true); }
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid() || !state.hasType(apvts.state.getType())) return;
        uiScale = float(state.getProperty("uiScale", 1.0f));
        menu = juce::jlimit(0, kMenus - 1, int(state.getProperty("menu", 2)));
        for (int i = 0; i < kMenus; ++i) page[i] = int(state.getProperty("page" + juce::String(i), 0));
        bank = juce::jlimit(0, 63, int(state.getProperty("bank", 0)));
        drawerOpen = bool(state.getProperty("drawer", false));
        presetName_ = state.getProperty("presetName", "init").toString();
        presetIndex_ = int(state.getProperty("presetIndex", 0));
        for (int L = 0; L < 2; ++L) {
            layerName_[size_t(L)] = state.getProperty("layerName" + juce::String(L), L == 0 ? presetName_ : juce::String("init")).toString();
            layerIndex_[size_t(L)] = int(state.getProperty("layerIndex" + juce::String(L), L == 0 ? presetIndex_ : 0));
            layerPart_[size_t(L)] = int(state.getProperty("layerPart" + juce::String(L), 0));
        }
        apvts.replaceState(state);
        presetSnap_ = currentSetting();
        for (int L = 0; L < 2; ++L) layerSnap_[size_t(L)] = layerValues(L);
        ab_[0] = ab_[1] = presetSnap_; abName_[0] = abName_[1] = presetName_; abSlot_ = 0;
        abLayers_[0] = abLayers_[1] = layerNames();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TandemProcessor(); }
