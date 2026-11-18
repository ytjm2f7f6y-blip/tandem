// Loads the installed Tandem Audio Unit the way Logic does (through Core Audio, not as C++), plays
// notes through it and checks that sound comes out: with the bus layout a host gives it, with the
// transport running, with the side chain input on, at Logic's block sizes, and after the host
// moves a parameter. Mac only; run after the component is installed:
//   au_host            (exits 1 if anything is silent that should not be)
#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdio>

namespace {

int fails = 0;
void check(bool ok, const juce::String& what) {
    std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", what.toRawUTF8());
    if (!ok) ++fails;
}

struct Head final : juce::AudioPlayHead {
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;
        p.setBpm(bpm);
        p.setPpqPosition(ppq);
        p.setIsPlaying(playing);
        p.setTimeSignature(juce::AudioPlayHead::TimeSignature{});
        return p;
    }
};

struct Result { double peak = 0.0, rms = 0.0; };

// Holds a chord for half the blocks, then lets go; the side chain (if it has channels) gets noise.
Result play(juce::AudioPluginInstance& p, Head& head, double sr, int block, double seconds, bool noiseIn, std::initializer_list<int> notes = {48, 55, 60}) {
    const int ins = p.getTotalNumInputChannels(), outs = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> buf(std::max(ins, outs), block);
    juce::Random rnd(7);
    Result r;
    double sum = 0.0;
    long n = 0;
    const int blocks = int(seconds * sr / block);
    for (int b = 0; b < blocks; ++b) {
        buf.clear();
        if (noiseIn)
            for (int c = 0; c < ins; ++c)
                for (int i = 0; i < block; ++i) buf.setSample(c, i, 0.3f * (rnd.nextFloat() * 2.0f - 1.0f));
        juce::MidiBuffer m;
        if (b == 0) for (int note : notes) m.addEvent(juce::MidiMessage::noteOn(1, note, 0.9f), 5);
        if (b == blocks / 2) for (int note : notes) m.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        p.processBlock(buf, m);
        if (head.playing) head.ppq += double(block) / sr * head.bpm / 60.0;
        for (int c = 0; c < outs; ++c)
            for (int i = 0; i < block; ++i) {
                const double v = buf.getSample(c, i);
                r.peak = std::max(r.peak, std::abs(v));
                sum += v * v;
                ++n;
            }
    }
    r.rms = std::sqrt(sum / double(std::max(1L, n)));
    return r;
}

juce::String busInfo(juce::AudioPluginInstance& p) {
    juce::String s;
    for (bool in : {true, false})
        for (int i = 0; i < p.getBusCount(in); ++i) {
            auto* b = p.getBus(in, i);
            s << (in ? "in " : "out ") << i << " \"" << b->getName() << "\" " << b->getLastEnabledLayout().getDescription()
              << (b->isEnabled() ? "" : " (off)") << ", " << b->getNumberOfChannels() << " ch; ";
        }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::AudioUnitPluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    const juce::String code = argc > 1 ? argv[1] : "Tndm";          // au_host TnSC: Tandem SC, the side chain test
    format.findAllTypesForFile(found, "AudioUnit:Synths/aumu," + code + ",SamP");
    if (found.isEmpty())
        for (const auto& id : format.searchPathsForPlugins({}, false, false))
            if (id.contains(code)) format.findAllTypesForFile(found, id);
    check(!found.isEmpty(), "the system finds it (aumu " + code + " SamP)");
    if (found.isEmpty()) return 1;
    const double sr = 48000.0;
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> p(format.createInstanceFromDescription(*found[0], sr, 512, error));
    check(p != nullptr, "it loads as an Audio Unit" + (error.isEmpty() ? juce::String() : ": " + error));
    if (!p) return 1;
    Head head;
    p->setPlayHead(&head);
    std::printf("      %d parameters; buses as loaded: %s\n", p->getParameters().size(), busInfo(*p).toRawUTF8());

    p->prepareToPlay(sr, 512);
    const Result a = play(*p, head, sr, 512, 1.5, false);
    std::printf("      as loaded: peak %.3f, rms %.4f\n", a.peak, a.rms);
    check(a.peak > 0.02, "a chord sounds, buses as loaded");

    head.playing = true;
    const Result b = play(*p, head, sr, 512, 1.5, false);
    check(b.peak > 0.02, juce::String::formatted("with the transport running (peak %.3f)", b.peak));
    head.playing = false;

    for (int bs : {64, 1156, 4096}) {          // Logic's live, odd and bounce sizes
        p->releaseResources();
        p->prepareToPlay(sr, bs);
        const Result r = play(*p, head, sr, bs, 1.5, false);
        check(r.peak > 0.02, juce::String::formatted("blocks of %d samples (peak %.3f)", bs, r.peak));
    }

    // The side chain on (Logic turns it on when a track is chosen in the plugin header), and off.
    for (const auto set : {juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled()}) {
        if (p->getBusCount(true) == 0) break;
        p->releaseResources();
        auto layout = p->getBusesLayout();
        layout.inputBuses.getReference(0) = set;
        const bool took = p->setBusesLayout(layout);
        p->prepareToPlay(sr, 512);
        const Result r = play(*p, head, sr, 512, 1.5, true);
        check(r.peak > 0.02, juce::String::formatted("side chain %s (%s; peak %.3f) %s", set.isDisabled() ? "off" : "on, noise in",
                                                      took ? "layout taken" : "layout refused", r.peak, busInfo(*p).toRawUTF8()));
    }

    // The host moves the cutoff: the sound changes.
    juce::AudioProcessorParameter* cutoff = nullptr;
    for (auto* q : p->getParameters())
        if (q->getName(40) == "L1 Cutoff" || q->getName(40) == "Cutoff") cutoff = q;
    check(cutoff != nullptr, "the host sees the cutoff parameter");
    if (cutoff) {
        p->releaseResources();
        p->prepareToPlay(sr, 512);
        cutoff->setValueNotifyingHost(1.0f);
        const Result open = play(*p, head, sr, 512, 1.0, false);
        cutoff->setValueNotifyingHost(0.15f);
        const Result shut = play(*p, head, sr, 512, 1.0, false);
        std::printf("      cutoff open: rms %.4f; nearly shut: rms %.4f (value now %.2f)\n", open.rms, shut.rms, cutoff->getValue());
        check(open.rms > 0.005 && shut.rms < open.rms * 0.6 && std::abs(cutoff->getValue() - 0.15f) < 0.01f, "the host's cutoff change reaches the sound");
    }
    p->releaseResources();
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
