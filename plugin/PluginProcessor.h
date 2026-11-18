#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Params.h"
#include "Presets.h"
#include "td_engine.hpp"

#include <array>
#include <atomic>
#include <vector>

#ifndef JucePlugin_Name
 #define JucePlugin_Name "Tandem"   // set by the plugin build; this covers test builds
#endif
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0"
#endif

class TandemProcessor final : public juce::AudioProcessor {
public:
    TandemProcessor();
    ~TandemProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override { return tpre::kNumFactory; }
    int getCurrentProgram() override { return juce::jlimit(0, tpre::kNumFactory - 1, presetIndex_); }
    void setCurrentProgram(int index) override { loadPreset(index); }
    const juce::String getProgramName(int index) override { return index >= 0 && index < tpre::kNumFactory ? juce::String(tpre::kFactory[index].name) : juce::String(); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- parameters (message thread), by layer key
    juce::RangedAudioParameter* param(const juce::String& key, int layer = 0) const;      // a layer key, "volume" or "setup_..."
    juce::RangedAudioParameter* volume() const { return volume_; }
    float value(const juce::String& key, int layer = 0) const;
    static void set(juce::RangedAudioParameter* p, float value);      // in the parameter's own units, host told

    // ---- presets: factory, then the person's own files (message thread)
    struct PresetEntry { juce::String name; bool factory = true; juce::File file; };
    std::vector<PresetEntry> presetList() const;
    void loadPreset(int index);
    void stepPreset(int delta);
    juce::String presetName() const { return presetName_; }
    int presetIndex() const { return presetIndex_; }
    bool presetEdited() const;
    juce::String saveUserPreset(const juce::String& name);
    bool deleteUserPreset(int index);
    static juce::File presetFolder();

    // ---- compare
    int abSlot() const { return abSlot_; }
    void selectAB(int slot);

    // ---- the window
    static constexpr int kMenus = 5;         // the display's menus: perf, voice, mod, arp, fx
    float uiScale = 0.8f;
    int menu = 2;
    int page[kMenus] = {0, 0, 0, 0, 0};      // the page in each
    int bank = 0;                            // the patch keys' bank, A-H
    bool drawerOpen = false;
    bool overdriven() const { return odrive_.load(std::memory_order_relaxed); }
    // The voices' lamps and the matrix's live output, as of the last block (the audio thread writes them).
    static constexpr int kMeterVoices = 16;
    float voiceMeter(int layer, int voice) const { return meters_[size_t(layer & 1)][size_t(juce::jlimit(0, kMeterVoices - 1, voice))].load(std::memory_order_relaxed); }
    int voiceCount(int layer) const { return vcount_[size_t(layer & 1)].load(std::memory_order_relaxed); }
    float modNow(int dest) const { return dest > 0 && dest < td::kNumModDests ? modNow_[size_t(dest)].load(std::memory_order_relaxed) : 0.0f; }
    int voicesSounding() const { return sounding_.load(std::memory_order_relaxed); }
    double sampleRate() const { return sampleRate_; }

    // For tests: the engine as it stands.
    td::Engine& engine() { return engine_; }

private:
    using Setting = std::vector<float>;
    Setting currentSetting() const;
    void applySetting(const Setting& s);
    bool isSoundParam(int index) const;

    td::Engine engine_;
    tp::LayerReader reader_[td::Engine::kLayers];
    juce::RangedAudioParameter* volume_ = nullptr;
    std::atomic<float>* volumeRaw_ = nullptr;
    struct Setup { std::atomic<float>* tune = nullptr, *transpose = nullptr, *velocity = nullptr, *pressure = nullptr, *ch1 = nullptr, *ch2 = nullptr, *age = nullptr, *pedal = nullptr; } setup_;
    int noteMap_[128];                       // the note each key started, so transposing while it is held cannot strand it
    float lastPedal_ = 0.0f;                 // the sustain pedal as last sent (0..1), and the polarity it was read with
    bool lastPedalInv_ = false;
    std::vector<juce::RangedAudioParameter*> all_;
    std::vector<juce::String> layerKeys_;

    int presetIndex_ = 0;
    juce::String presetName_ = "init";
    Setting presetSnap_;
    int abSlot_ = 0;
    Setting ab_[2];
    juce::String abName_[2];
    int abIndex_[2] = {0, 0};

    double sampleRate_ = 48000;
    std::vector<float> l_, r_;
    std::atomic<bool> odrive_{false};
    std::atomic<int> sounding_{0};
    std::array<std::array<std::atomic<float>, kMeterVoices>, 2> meters_{};
    std::array<std::atomic<int>, 2> vcount_{};
    std::array<std::atomic<float>, td::kNumModDests> modNow_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TandemProcessor)
};
