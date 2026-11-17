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

void shot(const juce::String& file, int preset, int menu, int page, const char* touched, const char* help, float scale) {
    TandemProcessor proc;
    proc.setRateAndBufferSizeDetails(48000, 1600);
    proc.prepareToPlay(48000, 1600);
    proc.loadPreset(preset);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    auto* te = dynamic_cast<TandemEditor*>(ed.get());
    te->setScale(scale);
    te->showMenu(menu, page);
    playChord(proc);
    if (touched) te->touch(touched);
    if (help) te->showHelp(help);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
    juce::File f(file);
    f.deleteFile();
    juce::FileOutputStream os(f);
    juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

}  // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String dir = argc > 1 ? argv[1] : ".";
    shot(dir + "/ui_init.png", 0, 0, 0, nullptr, "f_res", 1.0f);
    shot(dir + "/ui_vowel_pad_matrix.png", 5, 1, 0, nullptr, "enc_c", 1.0f);
    shot(dir + "/ui_sync_lead_touched.png", 2, 2, 6, "pitch_mod", "m_pitch", 1.0f);
    shot(dir + "/ui_80.png", 1, 0, 2, nullptr, nullptr, 0.8f);
    return 0;
}
