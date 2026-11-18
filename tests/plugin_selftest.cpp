// End-to-end: the real processor, driven the way a host drives it.
#include "PluginEditor.h"
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

        // The setup drawer: a MIDI channel, transposing while a key is held, the pedal turned round,
        // and none of it part of a sound.
        auto run = [&](juce::MidiBuffer m, int blocks) {
            p.processBlock(io, m);
            for (int b = 0; b < blocks; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        };
        auto note = [](int ch, int n, bool on) { juce::MidiBuffer m; m.addEvent(on ? juce::MidiMessage::noteOn(ch, n, 0.8f) : juce::MidiMessage::noteOff(ch, n), 0); return m; };
        TandemProcessor::set(p.param("setup_ch1"), 2.0f);
        run(note(1, 60, true), 4);
        const bool deaf = p.voicesSounding() == 0;
        run(note(2, 60, true), 4);
        const bool hears = p.voicesSounding() == 1;
        run(note(2, 60, false), 100);
        CHECK(deaf && hears && p.voicesSounding() == 0, "layer 1 listens on its own MIDI channel only");
        TandemProcessor::set(p.param("setup_ch1"), 0.0f);
        run(note(1, 60, true), 4);
        TandemProcessor::set(p.param("setup_transpose"), 7.0f);
        run(note(1, 60, false), 100);
        CHECK(p.voicesSounding() == 0, "transposing while a key is held cannot leave its note stuck");
        TandemProcessor::set(p.param("setup_pedal"), 1.0f);
        run(note(1, 62, true), 1);
        run(note(1, 62, false), 40);
        const bool held = p.voicesSounding() == 1;        // an inverted pedal reads "up" as down
        juce::MidiBuffer press;
        press.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        run(press, 100);
        CHECK(held && p.voicesSounding() == 0, "an inverted sustain pedal works the other way round");
        TandemProcessor::set(p.param("setup_pedal"), 0.0f);
        TandemProcessor::set(p.param("setup_transpose"), 0.0f);
        p.loadPreset(2);
        TandemProcessor::set(p.param("setup_age"), 0.9f);
        TandemProcessor::set(p.param("setup_tune"), 20.0f);
        CHECK(!p.presetEdited(), "the setup drawer is not part of a sound");
        TandemProcessor::set(p.param("setup_age"), 0.5f);
        TandemProcessor::set(p.param("setup_tune"), 0.0f);
    }
    {   // The voices' lamps, the matrix's live output, VARY and the panel's new keys.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        p.loadPreset(tpre::kNumFactory - 1);           // "dice keys": NOTE RND on the cutoff, a vibrato via the wheel, VARY
        CHECK(p.presetName() == "dice keys", "the new factory patch is the last one");
        juce::AudioBuffer<float> io(2, block);
        juce::MidiBuffer chord;
        for (int n : {48, 55, 60}) chord.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 0);
        p.processBlock(io, chord);
        for (int b = 0; b < 10; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        int lit = 0;
        for (int v = 0; v < TandemProcessor::kMeterVoices; ++v) lit += p.voiceMeter(0, v) > 0.003f;
        std::printf("      %d voice lamps lit of %d voices\n", lit, p.voiceCount(0));
        CHECK(p.voiceCount(0) > 0 && lit == std::min(3, p.voiceCount(0)), "a three-note chord lights the voices it plays");
        CHECK(p.modNow(td::kMdCutoff) != 0.0f, "the matrix's live output reaches the window (NOTE RND on the cutoff)");

        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* te = dynamic_cast<TandemEditor*>(ed.get());
        const float seed = p.value("vary_seed");
        te->rollDice();
        CHECK(p.value("vary_seed") != seed && p.presetEdited(), "DICE rolls a new variation, and the patch counts as changed");
        p.loadPreset(0);
        const bool made = te->assignFor("f_cutoff", 0.4f);
        const int slot = te->assignedSlot();
        CHECK(made && slot == 0 && juce::roundToInt(p.value("m1_dst")) == td::kMdCutoff && juce::roundToInt(p.value("m1_src")) == td::kMsLfo1
                  && std::abs(p.value("m1_amt") - 0.4f) < 0.01f,
              "ASSIGN then the cutoff knob: slot 1, LFO 1 to cutoff at the amount turned");
        const bool second = te->assignFor("f_res", -0.2f);
        CHECK(second && te->assignedSlot() == 1 && juce::roundToInt(p.value("m2_dst")) == td::kMdResonance, "a second ASSIGN takes the next free slot");
        CHECK(!te->assignFor("glide", 0.5f), "ASSIGN on a knob with no matrix destination makes no slot");
        te->openPageFor("dice");
        CHECK(p.menu == 0 && juce::String(TandemEditor::pages(0)[size_t(p.page[0])].title) == "VARIATION", "SHIFT + DICE opens the VARIATION page");
        te->openPageFor("vm1_2");
        CHECK(p.menu == 1 && juce::String(TandemEditor::pages(1)[size_t(p.page[1])].title) == "VOICE", "SHIFT + a voice mode lamp opens the VOICE page");
        CHECK(TandemEditor::pages(2).size() == size_t(td::kMatrixSlots) + 7, "twelve matrix pages, then note random and the rest");
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
