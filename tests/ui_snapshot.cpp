// Renders the window to PNG files after playing through the plugin, for checking it
// without a host:  ui_snapshot [folder] [scale]
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace {

void playChord(TandemProcessor& proc) {
    juce::AudioBuffer<float> io(2, 1600);
    for (int b = 0; b < 30; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) for (int n : {48, 55, 60, 64}) midi.addEvent(juce::MidiMessage::noteOn(1, n, 0.9f), 0);
        proc.processBlock(io, midi);
    }
}

struct Shot {
    const char* file;
    int preset, menu, page;
    const char* touched = nullptr;        // a parameter, as if just moved
    const char* shiftFor = nullptr;       // a control, as if SHIFT and it
    bool drawer = false, shiftLatched = false;
    float scale = 1.0f;
    bool assignLatched = false;
    const char* assignTo = nullptr;       // a control, as if ASSIGN and that knob turned
    const char* tagFor = nullptr;         // a control, its value tag showing
    const char* focusOn = nullptr;        // a control with keyboard focus
    int layer = 0;                        // the layer being edited
    int browse = -1;                      // a layer's sound browser open
};

float gMult = 1.0f;                       // ui_snapshot <folder> <scale>: every shot that much larger

void shot(const juce::String& dir, const Shot& s) {
    TandemProcessor proc;
    proc.setRateAndBufferSizeDetails(48000, 1600);
    proc.prepareToPlay(48000, 1600);
    proc.loadPreset(s.preset);
    proc.drawerOpen = s.drawer;
    proc.setEditLayer(s.layer);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* te = dynamic_cast<TandemEditor*>(ed.get());
    te->setScale(s.scale * gMult);
    te->showMenu(s.menu, s.page);
    playChord(proc);
    if (s.touched) te->touch(s.touched);
    if (s.shiftFor) te->openPageFor(s.shiftFor);
    if (s.shiftLatched) te->latchShift();
    if (s.assignTo) { te->assignFor(s.assignTo, 0.45f); te->touch(juce::String("m") + juce::String(te->assignedSlot() + 1) + "_amt"); }
    if (s.assignLatched) te->latchAssign();
    if (s.tagFor) te->showTagFor(s.tagFor);
    if (s.focusOn) te->focusControl(s.focusOn);
    if (s.browse >= 0) te->openBrowser(s.browse);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
    juce::File f(dir + "/" + s.file);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

}  // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String dir = argc > 1 ? argv[1] : ".";
    if (argc > 2) gMult = juce::jlimit(0.5f, 1.5f, juce::String(argv[2]).getFloatValue());
    shot(dir, {"ui_init.png", 0, 2, 0});
    shot(dir, {"ui_vowel_pad_filter_page.png", 5, 1, 0, nullptr, "f_cutoff"});
    shot(dir, {"ui_sync_lead_touched.png", 2, 4, 0, "pitch_mod"});
    shot(dir, {"ui_drawer.png", 1, 0, 0, nullptr, nullptr, true});
    shot(dir, {"ui_shift.png", 3, 2, 8, nullptr, nullptr, false, true});
    shot(dir, {"ui_80.png", 1, 1, 2, nullptr, nullptr, false, false, 0.8f});
    int dice = 0;
    for (int i = 0; i < tpre::kNumFactory; ++i) if (juce::String(tpre::kFactory[i].name) == "dice keys") dice = i;
    shot(dir, {"ui_dice_keys_matrix_via.png", dice, 2, 1});
    shot(dir, {"ui_assign_latched.png", dice, 2, 0, nullptr, nullptr, false, false, 1.0f, true});
    shot(dir, {"ui_shift_curve.png", dice, 2, 1, nullptr, nullptr, false, true});
    shot(dir, {"ui_vary_page.png", dice, 0, 8, "vary"});
    shot(dir, {"ui_matrix_overview_tag.png", dice, 2, 0, nullptr, nullptr, false, false, 1.0f, false, nullptr, "f_cutoff"});
    { Shot b{"ui_browser_layer2.png", 0, 2, 0}; b.layer = 1; b.browse = 1; shot(dir, b); }
    shot(dir, {"ui_keyboard_focus.png", 0, 1, 0, nullptr, nullptr, false, false, 1.0f, false, nullptr, nullptr, "f_res"});
    // Stage 2: both layers, the play modes, the arpeggiators.
    auto named = [](const char* n) { for (int i = 0; i < tpre::kNumFactory; ++i) if (juce::String(tpre::kFactory[i].name) == n) return i; return 0; };
    auto on = [](Shot s, int layer) { s.layer = layer; return s; };
    shot(dir, {"ui_split_bass_pad_zones.png", named("split bass pad"), 0, 1});
    shot(dir, on({"ui_split_arp_bass_layer2_arp.png", named("split arp bass"), 3, 0}, 1));
    shot(dir, {"ui_two_clocks_arp.png", named("two clocks"), 3, 0});
    shot(dir, on({"ui_cascade_growl_ext.png", named("cascade growl"), 1, 2, "2:ext_level"}, 1));
    shot(dir, {"ui_chaos_garden_rndmzr.png", named("chaos garden"), 0, 3});
    shot(dir, {"ui_side_chain_follower.png", named("side chain pad"), 0, 5});
    return 0;
}
