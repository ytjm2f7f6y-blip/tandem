// End-to-end: the real processor, driven the way a host drives it.
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cstdio>

#include <juce_audio_formats/juce_audio_formats.h>

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
        CHECK(n > 350 && p.param("cutoff", 1) != nullptr && p.param("perf_mode") != nullptr, "both layers' parameters, the play mode's and the volume are all there");
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
        int diceKeys = 0;
        for (int i = 0; i < tpre::kNumFactory; ++i) if (juce::String(tpre::kFactory[i].name) == "dice keys") diceKeys = i;
        p.loadPreset(diceKeys);                        // "dice keys": NOTE RND on the cutoff, a vibrato via the wheel, VARY
        CHECK(p.presetName() == "dice keys", "the dice keys patch is there");
        juce::AudioBuffer<float> io(2, block);
        juce::MidiBuffer chord;
        for (int n : {48, 55, 60}) chord.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 0);
        p.processBlock(io, chord);
        for (int b = 0; b < 10; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        int lit = 0;
        for (int v = 0; v < TandemProcessor::kMeterVoices; ++v) lit += p.voiceMeter(0, v) > 0.003f;
        std::printf("      %d voice lamps lit of %d voices\n", lit, p.voiceCount(0));
        CHECK(p.voiceCount(0) == 16 && lit == 6, "16 voices by default; in the stereo routing a three-note chord lights two lamps a note");
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
        CHECK(TandemEditor::pages(2).size() == size_t(td::kMatrixSlots) + 8 && juce::String(TandemEditor::pages(2)[0].title) == "MATRIX",
              "the MOD menu: the matrix overview, twelve slot pages, then note random and the rest");
    }
    {   // The critique's fixes: undo, lamps that can be clicked, the right-click menu's work, the keyboard and VoiceOver.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        CHECK(juce::roundToInt(p.value("voices")) == 3 && p.uiScale == 1.0f, "a new instance: 16 voices, the window at 100 %");
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* te = dynamic_cast<TandemEditor*>(ed.get());

        p.beginEdit("CUTOFF"); TandemProcessor::set(p.param("cutoff"), 0.3f); p.endEdit();
        p.beginEdit("RESONANCE"); TandemProcessor::set(p.param("resonance"), 0.6f); p.endEdit();
        te->doUndo();
        const bool undid = std::abs(p.value("resonance")) < 1e-4f && std::abs(p.value("cutoff") - 0.3f) < 1e-4f;
        te->doUndo();
        const bool undid2 = std::abs(p.value("cutoff") - 1.0f) < 1e-4f;
        te->doRedo(); te->doRedo();
        CHECK(undid && undid2 && std::abs(p.value("resonance") - 0.6f) < 1e-4f && std::abs(p.value("cutoff") - 0.3f) < 1e-4f,
              "UNDO takes back one edit at a time, REDO puts them back");
        p.beginEdit("PATCH"); p.loadPreset(3); p.endEdit();
        const auto loaded = p.presetName();
        te->doUndo();
        CHECK(loaded == "resonant bass" && p.presetName() == "init" && std::abs(p.value("resonance") - 0.6f) < 1e-4f,
              "UNDO after choosing a patch brings back the sound and the name before it");
        for (int i = 0; i < 5; ++i) { p.beginEdit("GLIDE"); TandemProcessor::set(p.param("glide"), 0.1f * float(i + 1)); p.endEdit(); }
        te->doUndo();
        CHECK(std::abs(p.value("glide")) < 1e-4f, "turning the same control again straight away is one step to undo");

        te->selectLampFor("f_route", 2);
        const bool route = juce::roundToInt(p.value("route")) == 2;
        te->selectLampFor("f_kt", 1);
        const bool full = juce::roundToInt(p.value("keytrack")) == 2;
        te->selectLampFor("f_kt", 1);
        const bool off = juce::roundToInt(p.value("keytrack")) == 0;
        te->selectLampFor("m_osc", 0);                  // both lit: OSC 1's lamp clicked takes it out
        const bool osc2only = juce::roundToInt(p.value("mod_osc")) == 2;
        te->selectLampFor("m_osc", 1);                  // and OSC 2 alone can't be taken out
        te->selectLampFor("o1_wave", 4);
        CHECK(route && full && off && osc2only && juce::roundToInt(p.value("mod_osc")) == 2 && juce::roundToInt(p.value("osc1_wave")) == 4,
              "clicking a lamp (or a ring's lamp) goes straight to its setting; a lit lamp clicked again turns it off");

        p.loadPreset(0);
        te->modulateFrom("f_res", td::kMsLfo2);
        const bool modded = juce::roundToInt(p.value("m1_dst")) == td::kMdResonance && juce::roundToInt(p.value("m1_src")) == td::kMsLfo2
                            && std::abs(p.value("m1_amt") - 0.25f) < 1e-3f;
        te->clearModulationOf("f_res");
        CHECK(modded && juce::roundToInt(p.value("m1_dst")) == 0 && juce::roundToInt(p.value("m1_src")) == 0,
              "right-click: Modulate from LFO 2 makes a slot; Clear its modulation empties it");
        te->doUndo();
        CHECK(juce::roundToInt(p.value("m1_dst")) == td::kMdResonance, "and UNDO brings the slot back");

        juce::Component* cutoff = nullptr;
        for (auto* c : te->getChildren()) if (c->getTitle() == "Cutoff") cutoff = c;
        const float before = p.param("cutoff")->getValue();
        if (cutoff) cutoff->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        const auto reading = te->accessibleReading("f_cutoff"), shown = te->accessibleReading("disp"), arp = te->accessibleReading("arp_on");
        std::printf("      VoiceOver: %s | %s | %s\n", reading.toRawUTF8(), arp.toRawUTF8(), shown.substring(0, 60).toRawUTF8());
        CHECK(cutoff && std::abs(p.param("cutoff")->getValue() - (before - 0.01f)) < 1e-4f && reading.startsWith("slider: Cutoff, ") && reading.endsWith("Hz")
                  && arp == "button: Arpeggiator, Off" && shown.startsWith("text: Display, MOD"),
              "the keyboard: Cutoff's stand-in takes the arrow keys, and VoiceOver hears what each control is and reads");
        int focusable = 0;
        for (auto* c : te->getChildren()) focusable += c->getWantsKeyboardFocus();
        std::printf("      %d controls on the keyboard's path\n", focusable);
        CHECK(focusable > 85, "Tab reaches every knob, fader and key");
    }

    {   // Stage 2: the second layer, the play modes, the arpeggiators, the side chain.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* te = dynamic_cast<TandemEditor*>(ed.get());
        juce::AudioBuffer<float> io(2, block);
        auto run = [&](juce::MidiBuffer m, int blocks) {
            p.processBlock(io, m);
            for (int b = 0; b < blocks; ++b) { juce::MidiBuffer none; p.processBlock(io, none); }
        };
        auto note = [](int n, bool on, int ch = 1) { juce::MidiBuffer m; m.addEvent(on ? juce::MidiMessage::noteOn(ch, n, 0.8f) : juce::MidiMessage::noteOff(ch, n), 0); return m; };
        auto sounding = [&](int L) { return p.engine().layer(L).soundingVoices(); };

        // LAYER 2: the panel edits layer 2, and in SINGLE it is the one that plays.
        te->pressControl("layer_2");
        const bool editing = p.editLayer() == 1 && p.param("cutoff")->getParameterID() == "l2_cutoff";
        TandemProcessor::set(p.param("cutoff"), 0.4f);
        run(note(60, true), 4);
        const bool l2 = sounding(1) > 0 && sounding(0) == 0;
        run(note(60, false), 100);
        CHECK(editing && l2 && std::abs(p.value("cutoff", 0) - 1.0f) < 1e-4f && std::abs(p.value("cutoff", 1) - 0.4f) < 1e-4f && p.presetEdited(),
              "LAYER 2: the panel edits layer 2 (layer 1 untouched), and SINGLE plays it");
        CHECK(!p.canUndo() || p.undoName() != "EDIT LAYER", "choosing a layer to edit is not an edit to undo");

        // SHIFT + LAYER 1 swaps them; SHIFT + LAYER 2 copies the edited layer onto the other.
        te->openPageFor("layer_1");
        const bool swapped = std::abs(p.value("cutoff", 0) - 0.4f) < 1e-4f && std::abs(p.value("cutoff", 1) - 1.0f) < 1e-4f;
        te->doUndo();
        const bool unswapped = std::abs(p.value("cutoff", 1) - 0.4f) < 1e-4f;
        te->openPageFor("layer_2");
        CHECK(swapped && unswapped && std::abs(p.value("cutoff", 0) - 0.4f) < 1e-4f, "SHIFT + LAYER 1 swaps the layers (and UNDO swaps them back); SHIFT + LAYER 2 copies");

        // The play mode ring: DUAL plays both.
        te->selectLampFor("play_mode", td::kPlayDual);
        run(note(60, true), 4);
        const bool dual = sounding(0) > 0 && sounding(1) > 0;
        run(note(60, false), 100);
        CHECK(juce::roundToInt(p.value("perf_mode")) == td::kPlayDual && dual, "the PLAY MODE ring: DUAL plays both layers");

        // ZONES: SHIFT + PLAY MODE, then a key sets the split.
        te->openPageFor("play_mode");
        const bool zones = p.menu == 0 && juce::String(TandemEditor::pages(0)[size_t(p.page[0])].title) == "ZONES" && p.learningSplit();
        run(note(72, true), 2);
        const int learnt = p.learnedSplit();
        run(note(72, false), 50);
        CHECK(zones && learnt == 72 && !p.learningSplit(), "SHIFT + PLAY MODE opens ZONES; the next key played is the split");

        // MIDI channels: each layer its own.
        TandemProcessor::set(p.param("setup_ch1"), 1.0f);
        TandemProcessor::set(p.param("setup_ch2"), 2.0f);
        run(note(48, true, 2), 4);
        const bool ch2 = sounding(1) > 0 && sounding(0) == 0;
        run(note(48, false, 2), 100);
        TandemProcessor::set(p.param("setup_ch1"), 0.0f);
        TandemProcessor::set(p.param("setup_ch2"), 0.0f);
        CHECK(ch2, "MIDI CH L2: a layer on its own channel takes that channel's notes, in any play mode");

        // The arpeggiator, on layer 1 alone, at the panel's own tempo.
        te->selectLampFor("play_mode", td::kPlaySingle);
        te->pressControl("layer_1");
        te->pressControl("arp_on");
        TandemProcessor::set(p.param("perf_host"), 0.0f);
        TandemProcessor::set(p.param("perf_tempo"), 120.0f);
        juce::MidiBuffer chord;
        for (int n : {60, 64, 67}) chord.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 0);
        p.processBlock(io, chord);
        int starts = 0;
        uint64_t last = 0;
        for (int b = 0; b < int(sr / block); ++b) {     // a second
            juce::MidiBuffer none;
            p.processBlock(io, none);
            uint64_t newest = 0;
            for (int i = 0; i < td::Layer::kMaxVoices; ++i) newest = std::max(newest, p.engine().layer(0).voice(i).age);
            if (newest != last) { ++starts; last = newest; }
        }
        std::printf("      the arpeggio: %d steps in a second at 120 BPM, 1/16\n", starts);
        CHECK(p.value("arp_on") >= 0.5f && starts >= 7 && starts <= 9 && p.arpPlaying(0), "ARP ON: a held chord plays as an arpeggio, eight steps a second at 120 BPM");
        juce::MidiBuffer upAll;
        for (int n : {60, 64, 67}) upAll.addEvent(juce::MidiMessage::noteOff(1, n), 0);
        run(upAll, 100);
        te->pressControl("arp_on");

        // TAP: taps half a second apart make 120 BPM, and take the clock from the host.
        TandemProcessor::set(p.param("perf_host"), 1.0f);
        te->tap(1000.0); te->tap(1500.0); te->tap(2000.0);
        CHECK(std::abs(p.value("perf_tempo") - 120.0f) < 0.5f && p.value("perf_host") < 0.5f, "TAP: taps half a second apart set 120 BPM, and HOST goes off");

        // The side chain: VCA ON/EXT opens with the input. (Only in builds with the side chain bus.)
        if (TandemProcessor().getBusCount(true) > 0) {
            TandemProcessor q;
            auto layout = q.getBusesLayout();
            layout.inputBuses.getReference(0) = juce::AudioChannelSet::stereo();
            const bool takes = q.setBusesLayout(layout);
            q.setRateAndBufferSizeDetails(sr, block);
            q.prepareToPlay(sr, block);
            TandemProcessor::set(q.param("vca_src"), 2.0f);
            TandemProcessor::set(q.param("perf_efrel"), 0.05f);
            juce::AudioBuffer<float> buf(2, block);
            juce::Random rnd(3);
            double withIn = 0, without = 0;
            juce::MidiBuffer on;
            on.addEvent(juce::MidiMessage::noteOn(1, 57, 0.9f), 0);
            for (int b = 0; b < 110; ++b) {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) buf.setSample(c, i, b < 30 ? 0.4f * (rnd.nextFloat() * 2 - 1) : 0.0f);
                juce::MidiBuffer m;
                if (b == 0) m = on;
                q.processBlock(buf, m);
                double s = 0;
                for (int i = 0; i < block; ++i) s += double(buf.getSample(0, i)) * buf.getSample(0, i);
                if (b > 10 && b < 30) withIn += s; else if (b >= 90) without += s;      // half a second after it stops
            }
            std::printf("      side chain: %.4f with the input, %.6f without\n", std::sqrt(withIn / (19.0 * block)), std::sqrt(without / (20.0 * block)));
            CHECK(takes && q.sideChainConnected() && withIn > 1e-3 && without < withIn * 1e-3, "the side chain (EXT IN): with VCA ON/EXT, a held note sounds while the input does");
        }

        // The mouse, as a host delivers it: a drag up on a knob turns it; a click on a ring steps it.
        {
            auto at = [](const char* id) {
                for (int i = 0; i < layout::kNumControls; ++i)
                    if (juce::String(layout::kControls[i].id) == id) return juce::Point<float>(layout::kControls[i].x, layout::kControls[i].y);
                return juce::Point<float>();
            };
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            const auto t0 = juce::Time::getCurrentTime();
            auto ev = [&](juce::Point<float> pos, juce::Point<float> down, bool dragged) {
                return juce::MouseEvent(src, pos, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, te, te, t0, down,
                                        t0, 1, dragged);
            };
            auto drag = [&](const char* id, float up) {
                const auto c = at(id);
                te->mouseDown(ev(c, c, false));
                for (int k = 1; k <= 4; ++k) te->mouseDrag(ev(c.translated(0, -up * float(k) / 4.0f), c, true));
                te->mouseUp(ev(c.translated(0, -up), c, true));
            };
            te->pressControl("layer_1");
            TandemProcessor::set(p.param("resonance"), 0.0f);
            drag("f_res", 44.0f);
            const float res = p.value("resonance");
            te->pressControl("layer_2");
            TandemProcessor::set(p.param("resonance"), 0.0f);
            drag("f_res", 22.0f);
            const float res2 = p.value("resonance", 1), res1 = p.value("resonance", 0);
            const int wave = juce::roundToInt(p.value("osc1_wave"));
            const auto w = at("o1_wave");
            te->mouseDown(ev(w, w, false));
            te->mouseUp(ev(w, w, false));
            std::printf("      a 44-point drag up on RESONANCE: %.3f; on layer 2, 22 points: %.3f (layer 1 kept %.3f)\n", res, res2, res1);
            CHECK(std::abs(res - 0.2f) < 0.01f && std::abs(res2 - 0.1f) < 0.01f && std::abs(res1 - 0.2f) < 0.01f
                      && juce::roundToInt(p.value("osc1_wave")) == (wave + 1) % 12,
                  "the mouse: a drag turns a knob of the layer being edited; a click steps a ring");
            te->pressControl("layer_1");
        }

        // What the window draws: a knob turned to a value shows the frame of its strip for that value
        // (checked by matching the drawn knob against every frame, on whatever renderer this machine uses).
        {
            juce::SharedResourcePointer<TandemArt> art;
            int ci = -1;
            for (int i = 0; i < layout::kNumControls; ++i) if (juce::String(layout::kControls[i].id) == "f_res") ci = i;
            const auto& c = layout::kControls[ci];
            const auto& strip = art->strip[c.strip];
            const int frames = art->stripFrames[c.strip], fw = strip.getWidth();
            const float box = 2.0f * layout::kKnobRadius[c.kind] + 2.0f * layout::kShadowMargin;
            const int bw = juce::roundToInt(box), x0 = juce::roundToInt(c.x - box / 2), y0 = juce::roundToInt(c.y - box / 2);
            const float rr = layout::kKnobRadius[c.kind] * 0.8f;
            std::vector<juce::Image> scaled;
            for (int f = 0; f < frames; ++f)
                scaled.push_back(strip.getClippedImage({0, f * fw, fw, fw}).rescaled(bw, bw, juce::Graphics::mediumResamplingQuality)
                                     .convertedToFormat(juce::Image::ARGB));
            te->pressControl("layer_1");
            te->setScale(1.0f);
            juce::String seen;
            bool right = frames > 10;
            for (float v : {0.0f, 0.3f, 0.5f, 0.8f, 1.0f}) {
                TandemProcessor::set(p.param("resonance"), v);
                const auto snap = te->createComponentSnapshot(te->getLocalBounds(), true, 1.0f).convertedToFormat(juce::Image::ARGB);
                const juce::Image::BitmapData sd(snap, juce::Image::BitmapData::readOnly);
                int best = -1;
                double bestErr = 1e300;
                for (int f = 0; f < frames; ++f) {
                    const juce::Image::BitmapData fd(scaled[size_t(f)], juce::Image::BitmapData::readOnly);
                    double err = 0;
                    for (int y = 0; y < bw; ++y)
                        for (int x = 0; x < bw; ++x) {
                            const float dx = float(x) + 0.5f - box / 2, dy = float(y) + 0.5f - box / 2;
                            if (dx * dx + dy * dy > rr * rr || x0 + x >= sd.width || y0 + y >= sd.height) continue;
                            const auto a = sd.getPixelColour(x0 + x, y0 + y), b = fd.getPixelColour(x, y);
                            err += std::abs(int(a.getRed()) - int(b.getRed())) + std::abs(int(a.getGreen()) - int(b.getGreen())) + std::abs(int(a.getBlue()) - int(b.getBlue()));
                        }
                    if (err < bestErr) { bestErr = err; best = f; }
                }
                const int want = juce::roundToInt(v * float(frames - 1));
                seen << juce::String(v, 1) << ": frame " << best << " (" << want << ")  ";
                right = right && std::abs(best - want) <= 1;
            }
            std::printf("      RESONANCE as drawn: %s of %d frames, %dx%d snapshot\n", seen.toRawUTF8(), frames, te->getWidth(), te->getHeight());
            CHECK(right, "the knob drawn shows the strip's frame for its value");

            // Turning a knob in a real window: the window only repaints the areas it asks for, one at a
            // time, as the system does. After a drag the knob on the screen must be the knob as it now is.
            juce::Image screen(juce::Image::ARGB, te->getWidth(), te->getHeight(), true);
            std::vector<juce::Rectangle<int>> dirty;
            te->repaintHook = [&](juce::Rectangle<int> r) { dirty.push_back(r); };
            auto paintDirty = [&] {
                for (const auto& r : dirty) { juce::Graphics g(screen); g.reduceClipRegion(r); te->paint(g); }
                dirty.clear();
            };
            auto frame = [&] { te->tick(); paintDirty(); };
            { juce::Graphics g(screen); te->paint(g); }
            for (int i = 0; i < 5; ++i) frame();
            auto at = [](const char* id) {
                for (int i = 0; i < layout::kNumControls; ++i)
                    if (juce::String(layout::kControls[i].id) == id) return juce::Point<float>(layout::kControls[i].x, layout::kControls[i].y);
                return juce::Point<float>();
            };
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            const auto t0 = juce::Time::getCurrentTime();
            auto ev = [&](juce::Point<float> pos, juce::Point<float> down, bool dragged) {
                return juce::MouseEvent(src, pos, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, te, te, t0, down, t0, 1, dragged);
            };
            double worst = 0.0;
            for (const char* id : {"f_res", "f_cutoff", "e2_vel", "level2", "fx_fb", "m_detune"}) {
                const auto c0 = at(id);
                te->mouseDown(ev(c0, c0, false));
                paintDirty();
                frame();
                // The system paints what a mouse event asked for straight away, before the timer's next frame.
                for (int k = 1; k <= 12; ++k) { te->mouseDrag(ev(c0.translated(0, -5.0f * float(k)), c0, true)); paintDirty(); frame(); }
                te->mouseUp(ev(c0.translated(0, -60.0f), c0, true));
                paintDirty();
                for (int i = 0; i < 40; ++i) frame();          // the value tag comes and goes
                const auto fresh = te->createComponentSnapshot(te->getLocalBounds(), true, 1.0f).convertedToFormat(juce::Image::ARGB);
                const juce::Image::BitmapData a(screen, juce::Image::BitmapData::readOnly), b(fresh, juce::Image::BitmapData::readOnly);
                double diff = 0;
                int n = 0;
                for (int y = int(c0.y) - 18; y <= int(c0.y) + 18; ++y)
                    for (int x = int(c0.x) - 18; x <= int(c0.x) + 18; ++x, ++n) {
                        const auto p1 = a.getPixelColour(x, y), p2 = b.getPixelColour(x, y);
                        diff += std::abs(int(p1.getRed()) - int(p2.getRed())) + std::abs(int(p1.getGreen()) - int(p2.getGreen())) + std::abs(int(p1.getBlue()) - int(p2.getBlue()));
                    }
                diff /= double(n) * 3.0;
                std::printf("      %-9s after a drag: on screen vs as it is, %.2f levels apart on average\n", id, diff);
                worst = std::max(worst, diff);
            }
            te->repaintHook = nullptr;
            CHECK(worst < 2.0, "turning a knob, the knob on the screen follows (the window repaints all of it, not a sliver)");
        }

        // A factory sound can set layer 2 ("2:key") and the play mode.
        bool both = false;
        for (int i = 0; i < tpre::kNumFactory; ++i) {
            p.loadPreset(i);
            if (juce::roundToInt(p.value("perf_mode")) != td::kPlaySingle) { both = true; break; }
        }
        CHECK(both, "factory sounds that use both layers");
    }
    {   // A sound on each layer: any patch on either; either layer of a two-layer sound; saved and recalled.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* te = dynamic_cast<TandemEditor*>(ed.get());
        const auto list = p.presetList();
        auto find = [&](const char* n) { for (int i = 0; i < int(list.size()); ++i) if (list[size_t(i)].name == n) return i; return -1; };
        p.loadPreset(find("warm pad"));
        const float padCut = p.value("cutoff", 0);
        p.loadIntoLayer(find("reese"), 1);
        juce::String reeseKeys;
        bool matches = true;
        for (const auto& kv : tpre::kFactory[find("reese")].s) {
            if (kv.key == nullptr) break;
            matches = matches && std::abs(p.value(kv.key, 1) - kv.v) < 1e-3f;
        }
        CHECK(matches && std::abs(p.value("cutoff", 0) - padCut) < 1e-5f && p.layerName(0) == "warm pad" && p.layerName(1) == "reese"
                  && p.presetName() == "warm pad + reese" && !p.layerEdited(1),
              "a patch on layer 2 (reese under warm pad): layer 2 takes it all, layer 1 keeps its own");
        const int was = p.layerIndex(1);
        p.stepLayer(1, 1);
        CHECK(p.layerIndex(1) == was + 1 && p.layerName(1) == list[size_t(was + 1)].name, "the arrows step layer 2 to the next patch");
        p.loadIntoLayer(find("split bass pad"), 0, 1);       // the bass half of a two-layer sound, on layer 1
        CHECK(juce::roundToInt(p.value("voice_mode", 0)) == 1 && juce::roundToInt(p.value("octave", 0)) == 1 && p.layerName(0) == "split bass pad (2)",
              "either layer of a two-layer sound can go on a layer (split bass pad's bass, on layer 1)");
        TandemProcessor::set(p.param("resonance", 0), 0.77f);
        CHECK(p.layerEdited(0) && !p.layerEdited(1), "a layer knows when it has been changed (its window shows a *)");
        const auto err = p.saveLayerPatch("selftest bass", 0);
        const int mine = [&] { const auto l2 = p.presetList(); for (int i = 0; i < int(l2.size()); ++i) if (l2[size_t(i)].name == "selftest bass") return i; return -1; }();
        p.loadPreset(0);
        p.loadIntoLayer(mine, 1);
        const bool recalled = std::abs(p.value("resonance", 1) - 0.77f) < 1e-4f && juce::roundToInt(p.value("voice_mode", 1)) == 1 && p.layerName(1) == "selftest bass";
        juce::MemoryBlock mb;
        p.getStateInformation(mb);
        TandemProcessor q;
        q.setStateInformation(mb.getData(), int(mb.getSize()));
        CHECK(err.isEmpty() && mine >= 0 && !p.presetList()[size_t(mine)].bothLayers && recalled && q.layerName(1) == "selftest bass" && q.layerName(0) == "init",
              "a layer written as a patch comes back on the other layer; the layers' names are kept with the session");
        p.deleteUserPreset(mine);
        // The windows: the right arrow of layer 2's steps it, and UNDO takes it back.
        p.loadPreset(find("warm pad"));
        const auto w2 = juce::Point<float>(layout::kPatchWin[1][0] + layout::kPatchWin[1][2] - 6.0f, layout::kPatchWin[1][1] + layout::kPatchWin[1][3] / 2);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto t0 = juce::Time::getCurrentTime();
        const juce::MouseEvent click(src, w2, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, te, te, t0, w2, t0, 1, false);
        te->mouseDown(click);
        te->mouseUp(click);
        const auto stepped = p.layerName(1);
        const bool editing2 = p.editLayer() == 1;
        te->doUndo();
        CHECK(stepped != "init" && editing2 && p.layerName(1) == "init", "clicking layer 2's arrow puts the next patch on it (and edits it); UNDO takes it back");
    }
    {   // The matrix overview: drag a line for its amount, scroll over it, click it for its page; copy a slot to the other layer.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* te = dynamic_cast<TandemEditor*>(ed.get());
        TandemProcessor::set(p.param("m1_src", 0), float(td::kMsLfo1));
        TandemProcessor::set(p.param("m1_dst", 0), float(td::kMdCutoff));
        TandemProcessor::set(p.param("m1_amt", 0), 0.25f);
        p.menu = 2; p.page[2] = 0;                          // the overview
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto t0 = juce::Time::getCurrentTime();
        auto ev = [&](juce::Point<float> at, juce::Point<float> down) {
            return juce::MouseEvent(src, at, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f, te, te, t0, down, t0, 1, at != down);
        };
        const juce::Point<float> line(layout::kDispX + 60, layout::kDispY + 26 + 8);
        te->mouseDown(ev(line, line));
        te->mouseDrag(ev(line.translated(30, -30), line));
        te->mouseUp(ev(line.translated(30, -30), line));
        const float dragged = p.value("m1_amt", 0);
        CHECK(dragged > 0.4f && p.page[2] == 0, "dragging a line of the matrix overview turns its amount up (and stays on the overview)");
        te->mouseWheelMove(ev(line, line), juce::MouseWheelDetails{0.0f, -0.25f, false, false, false});
        CHECK(p.value("m1_amt", 0) < dragged, "scrolling over the line turns it down");
        te->mouseDown(ev(line, line));
        te->mouseUp(ev(line, line));
        CHECK(p.menu == 2 && p.page[2] == TandemEditor::kMatrixFirst, "a click on the line opens its slot's page");
        TandemProcessor::set(p.param("m1_src", 1), float(td::kMsEnv2));                   // layer 2's slot 1 is taken
        TandemProcessor::set(p.param("m1_dst", 1), float(td::kMdPan));
        te->copySlotToOtherLayer(0);
        CHECK(juce::roundToInt(p.value("m2_src", 1)) == td::kMsLfo1 && juce::roundToInt(p.value("m2_dst", 1)) == td::kMdCutoff
                  && std::abs(p.value("m2_amt", 1) - p.value("m1_amt", 0)) < 1e-4f && juce::roundToInt(p.value("m1_src", 1)) == td::kMsEnv2,
              "a slot copied to the other layer goes into its first free slot");
        te->doUndo();
        CHECK(juce::roundToInt(p.value("m2_src", 1)) == td::kMsNone, "UNDO takes the copy back");
    }
    {   // Your own waves: imported from a file into a slot, played, kept with the session and with a saved patch.
        TandemProcessor p;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("tandem_selftest_third.wav");
        tmp.deleteFile();
        {   // one cycle of the third harmonic, 600 points, at half scale and off centre
            juce::AudioBuffer<float> cyc(1, 600);
            for (int i = 0; i < 600; ++i) cyc.setSample(0, i, 0.1f + 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 3.0f * float(i) / 600.0f));
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os(tmp.createOutputStream());
            auto w = wav.createWriterFor(os, juce::AudioFormatWriterOptions().withSampleRate(44100.0).withNumChannels(1).withBitsPerSample(24));
            if (w) { w->writeFromAudioSampleBuffer(cyc, 0, 600); }
        }
        const int k = p.freeUserWave();
        const auto err = p.importWave(k, tmp);
        auto corr3 = [](const td::WaveTable* t) {                // how much like a sine of the third harmonic, -1..1
            if (!t) return 0.0;
            const float* x = t->level(0);
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < td::WaveTable::kSize; ++i) {
                const double y = std::sin(2.0 * juce::MathConstants<double>::pi * 3.0 * i / td::WaveTable::kSize);
                sxy += x[i] * y; sxx += double(x[i]) * x[i]; syy += y * y;
            }
            return sxy / std::sqrt(std::max(1e-12, sxx * syy));
        };
        const auto* t = p.engine().userWaves().t[size_t(k)].load();
        CHECK(err.isEmpty() && k == 0 && p.userWaveName(0) == "tandem_selftest_third" && corr3(t) > 0.995,
              "a single-cycle WAV becomes your wave U1 (resampled, centred, its name kept)");
        TandemProcessor::set(p.param("osc1_wave", 0), float(td::kWAlt1));             // OSC 1 on its first alternative slot...
        TandemProcessor::set(p.param("osc1_alt1", 0), float(td::WaveBank::kNumAlt + k)); // ...which holds your wave
        const Stats st = play(p, sr, block, {57}, 0.4, 0.2);
        CHECK(st.finite && st.peak > 0.02, "OSC 1 plays your wave");
        {   // the editor shows it by name
            std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
            juce::ignoreUnused(ed);
        }
        juce::MemoryBlock mb;
        p.getStateInformation(mb);
        TandemProcessor q;
        q.setStateInformation(mb.getData(), int(mb.getSize()));
        CHECK(q.userWaveName(0) == "tandem_selftest_third" && corr3(q.engine().userWaves().t[0].load()) > 0.995
                  && juce::roundToInt(q.value("osc1_alt1", 0)) == td::WaveBank::kNumAlt,
              "your wave is kept with the session");
        const auto saveErr = p.saveLayerPatch("selftest own wave", 0);
        const int mine = [&] { const auto l = p.presetList(); for (int i = 0; i < int(l.size()); ++i) if (l[size_t(i)].name == "selftest own wave") return i; return -1; }();
        // A different wave of yours sits in U1 now, and layer 1 uses it: the patch's wave must go elsewhere.
        std::vector<float> saw(TandemProcessor::kCycle);
        for (int i = 0; i < TandemProcessor::kCycle; ++i) saw[size_t(i)] = 1.0f - 2.0f * float(i) / float(TandemProcessor::kCycle);
        p.setUserWave(0, "another", saw);
        p.loadIntoLayer(mine, 1);
        const int to = juce::roundToInt(p.value("osc1_alt1", 1)) - td::WaveBank::kNumAlt;
        CHECK(saveErr.isEmpty() && mine >= 0 && to > 0 && to < td::kUserWaves && p.userWaveName(0) == "another"
                  && p.userWaveName(to) == "tandem_selftest_third" && corr3(p.engine().userWaves().t[size_t(to)].load()) > 0.995,
              "a saved patch brings its wave along; another wave of yours in use keeps its slot (the patch's goes to the next)");
        p.loadIntoLayer(mine, 0);                              // the same wave again: shared, not copied
        CHECK(juce::roundToInt(p.value("osc1_alt1", 0)) == td::WaveBank::kNumAlt + to && p.userWaveName(0) == "another",
              "the same wave twice is shared");
        p.deleteUserPreset(mine);
        CHECK(p.importWave(3, juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("no such file.wav")).isNotEmpty()
                  && p.userWaveName(3).isEmpty(),
              "a file Tandem can't read is turned down, and the slot stays as it was");
        tmp.deleteFile();
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
