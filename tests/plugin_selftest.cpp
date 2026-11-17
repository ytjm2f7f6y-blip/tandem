// End-to-end: the real processor, driven the way a host drives it.
#include "PluginProcessor.h"

#include <cstdio>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } else std::printf("ok:   %s\n", msg); } while (0)

namespace {

struct Stats { double peak = 0, rms = 0; bool finite = true; };

// Plays a chord (or one note) for `hold` seconds, lets go, and measures the output.
Stats play(TandemProcessor& p, double sr, int block, std::initializer_list<int> notes, double hold, double tail) {
    juce::AudioBuffer<float> io(2, block);
    Stats s;
    double sum = 0;
    long count = 0;
    const int nHold = int(hold * sr / block), nTail = int(tail * sr / block);
    for (int b = 0; b < nHold + nTail; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 17);
        if (b == nHold) for (int n : notes) midi.addEvent(juce::MidiMessage::noteOff(1, n), 3);
        io.clear();
        p.processBlock(io, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i) {
                const float v = io.getSample(c, i);
                if (!std::isfinite(v)) s.finite = false;
                s.peak = std::max(s.peak, double(std::abs(v)));
                sum += double(v) * v;
                ++count;
            }
    }
    s.rms = std::sqrt(sum / double(std::max(1L, count)));
    return s;
}

}  // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 48000;
    const int block = 512;
    {
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        const int n = p.getParameters().size();
        std::printf("      %d parameters\n", n);
        CHECK(n > 120, "a layer's parameters and the volume are all there");
        CHECK(p.getLatencySamples() == 24, "the latency the host is told is the decimator's (24 samples at 48 kHz)");

        const Stats quiet = play(p, sr, block, {}, 0.2, 0.0);
        CHECK(quiet.peak == 0.0, "silent until a note is played");
        const Stats one = play(p, sr, block, {60}, 1.0, 0.6);
        std::printf("      init, one note: peak %.3f, rms %.4f\n", one.peak, one.rms);
        CHECK(one.finite && one.peak > 0.05 && one.peak < 1.0, "init plays a note at a sensible level");

        // Every factory preset plays a chord without blowing up or going silent.
        bool ok = true;
        for (int i = 0; i < tpre::kNumFactory; ++i) {
            p.loadPreset(i);
            p.prepareToPlay(sr, block);
            const Stats st = play(p, sr, block, {48, 55, 60, 64}, 1.5, 1.0);
            std::printf("      %-16s peak %.3f  rms %.4f  %s\n", p.presetName().toRawUTF8(), st.peak, st.rms, p.presetEdited() ? "(edited?)" : "");
            ok = ok && st.finite && st.peak > 0.02 && st.peak < 1.6 && !p.presetEdited();
        }
        CHECK(ok, "every factory preset plays a chord, finite and at a sensible level");

        // State: save, change, restore.
        p.loadPreset(3);
        TandemProcessor::set(p.param("cutoff"), 0.3f);
        juce::MemoryBlock mb;
        p.getStateInformation(mb);
        TandemProcessor q;
        q.setStateInformation(mb.getData(), int(mb.getSize()));
        CHECK(std::abs(q.value("cutoff") - 0.3f) < 1e-4f && q.presetName() == p.presetName(), "the state Logic saves comes back the same");

        // Compare: A and B hold different sounds.
        p.loadPreset(0);
        p.selectAB(1);
        TandemProcessor::set(p.param("resonance"), 0.8f);
        p.selectAB(0);
        const bool aBack = p.value("resonance") < 0.01f;
        p.selectAB(1);
        CHECK(aBack && std::abs(p.value("resonance") - 0.8f) < 1e-3f, "A and B keep their own settings");

        // Sustain pedal and pitch bend reach the engine.
        p.loadPreset(0);
        juce::AudioBuffer<float> io(2, block);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 1);
        midi.addEvent(juce::MidiMessage::noteOff(1, 60), 200);
        p.processBlock(io, midi);
        for (int b = 0; b < 40; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        CHECK(p.voicesSounding() == 1, "the sustain pedal holds a released note");
        juce::MidiBuffer up;
        up.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
        p.processBlock(io, up);
        for (int b = 0; b < 100; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        CHECK(p.voicesSounding() == 0, "lifting the pedal lets it go");
    }
    {   // 44.1 and 96 kHz, small and odd block sizes.
        bool ok = true;
        for (double rate : {44100.0, 96000.0})
            for (int bs : {32, 333, 1024}) {
                TandemProcessor p;
                p.setRateAndBufferSizeDetails(rate, bs);
                p.prepareToPlay(rate, bs);
                p.loadPreset(1);
                const Stats st = play(p, rate, bs, {57, 64}, 0.5, 0.3);
                ok = ok && st.finite && st.peak > 0.02;
            }
        CHECK(ok, "plays at 44.1 and 96 kHz with blocks of 32, 333 and 1024 samples");
    }
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
