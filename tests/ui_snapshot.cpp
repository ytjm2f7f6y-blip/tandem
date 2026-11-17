// Renders the window to PNG files after playing through the plugin, for checking it
// without a host:  ui_snapshot [folder]
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
};

void shot(const juce::String& dir, const Shot& s) {
    TandemProcessor proc;
    proc.setRateAndBufferSizeDetails(48000, 1600);
    proc.prepareToPlay(48000, 1600);
    proc.loadPreset(s.preset);
    proc.drawerOpen = s.drawer;
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* te = dynamic_cast<TandemEditor*>(ed.get());
    te->setScale(s.scale);
    te->showMenu(s.menu, s.page);
    playChord(proc);
    if (s.touched) te->touch(s.touched);
    if (s.shiftFor) te->openPageFor(s.shiftFor);
    if (s.shiftLatched) te->latchShift();
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
    shot(dir, {"ui_init.png", 0, 2, 0});
    shot(dir, {"ui_vowel_pad_filter_page.png", 5, 1, 0, nullptr, "f_cutoff"});
    shot(dir, {"ui_sync_lead_touched.png", 2, 4, 0, "pitch_mod"});
    shot(dir, {"ui_drawer.png", 1, 0, 0, nullptr, nullptr, true});
    shot(dir, {"ui_shift.png", 3, 2, 8, nullptr, nullptr, false, true});
    shot(dir, {"ui_80.png", 1, 1, 2, nullptr, nullptr, false, false, 0.8f});
    return 0;
}
