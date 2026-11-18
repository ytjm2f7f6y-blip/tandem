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

    // ---- parameters (message thread), by key: a layer key ("cutoff"), "perf_...", "setup_..." or "volume".
    // A layer key is the edited layer's unless it says which: "2:cutoff", or layer 0 / 1.
    juce::RangedAudioParameter* param(const juce::String& key, int layer = -1) const;
    juce::RangedAudioParameter* volume() const { return volume_; }
    float value(const juce::String& key, int layer = -1) const;
    int editLayer() const { return juce::jlimit(0, 1, juce::roundToInt(editLayerRaw_->load(std::memory_order_relaxed))); }
    void setEditLayer(int layer);
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

    // ---- undo and redo (message thread): the sound before and after each edit made in the window.
    // Patch, compare and DICE are edits too. Wheel nudges to the same control within a second merge.
    void beginEdit(const juce::String& what);
    void endEdit();
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    juce::String undoName() const { return undo_.empty() ? juce::String() : undo_.back().what; }
    juce::String redoName() const { return redo_.empty() ? juce::String() : redo_.back().what; }

    // ---- the window
    static constexpr int kMenus = 5;         // the display's menus: perf, voice, mod, arp, fx
    float uiScale = 1.0f;                    // 100 %: the smallest print is 8 pt
    int menu = 2;
    int page[kMenus] = {0, 0, 0, 0, 0};      // the page in each
    int bank = 0;                            // the patch keys' bank, A-H
    bool drawerOpen = false;
    bool overdriven() const { return odrive_.load(std::memory_order_relaxed); }
    // SHIFT + PLAY MODE: the next key played sets the split point (the window applies it).
    void learnSplit(bool on) { learnNote_.store(-1); learning_.store(on); }
    bool learningSplit() const { return learning_.load(); }
    int learnedSplit() { const int n = learnNote_.exchange(-1); return n; }
    // The clock, as of the last block: its beat and tempo; whether layer i's arpeggiator is playing.
    double clockBeat() const { return beat_.load(std::memory_order_relaxed); }
    float clockTempo() const { return tempo_.load(std::memory_order_relaxed); }
    bool arpPlaying(int layer) const { return arpRun_[size_t(layer & 1)].load(std::memory_order_relaxed); }
    bool sideChainConnected() const { return sideOn_.load(std::memory_order_relaxed); }
    float follower() const { return follow_.load(std::memory_order_relaxed); }
    // The voices' lamps and the matrix's live output, as of the last block (the audio thread writes them).
    // One lamp per voice as the VOICES setting counts them: in the stereo routing a note takes two.
    static constexpr int kMeterVoices = 16;
    float voiceMeter(int layer, int voice) const { return meters_[size_t(layer & 1)][size_t(juce::jlimit(0, kMeterVoices - 1, voice))].load(std::memory_order_relaxed); }
    int voiceCount(int layer) const { return vcount_[size_t(layer & 1)].load(std::memory_order_relaxed); }
    float modNow(int dest) const { return dest > 0 && dest < td::kNumModDests ? modNow_[size_t(dest)].load(std::memory_order_relaxed) : 0.0f; }     // the edited layer's
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
    tp::PerfReader perfReader_;
    std::atomic<float>* editLayerRaw_ = nullptr;
    juce::RangedAudioParameter* volume_ = nullptr;
    std::atomic<float>* volumeRaw_ = nullptr;
    struct Setup { std::atomic<float>* tune = nullptr, *transpose = nullptr, *velocity = nullptr, *pressure = nullptr, *ch1 = nullptr, *ch2 = nullptr, *age = nullptr, *pedal = nullptr; } setup_;
    int noteMap_[16 * 128];                  // the note each key (per channel) started, so transposing while it is held cannot strand it
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
    struct Snapshot { Setting values, snap; juce::String name; int index = 0; };
    struct UndoStep { juce::String what; Snapshot before, after; double when = 0; };
    Snapshot snapshot() const { return {currentSetting(), presetSnap_, presetName_, presetIndex_}; }
    void restore(const Snapshot& s);
    std::vector<UndoStep> undo_, redo_;
    Snapshot pending_;
    juce::String pendingWhat_;
    bool editing_ = false;

    double sampleRate_ = 48000;
    std::vector<float> l_, r_, scL_, scR_;
    std::atomic<bool> learning_{false};
    std::atomic<int> learnNote_{-1};
    std::atomic<double> beat_{0.0};
    std::atomic<float> tempo_{120.0f}, follow_{0.0f};
    std::array<std::atomic<bool>, 2> arpRun_{};
    std::atomic<bool> sideOn_{false};
    std::atomic<bool> odrive_{false};
    std::atomic<int> sounding_{0};
    std::array<std::array<std::atomic<float>, kMeterVoices>, 2> meters_{};
    std::array<std::atomic<int>, 2> vcount_{};
    std::array<std::atomic<float>, td::kNumModDests> modNow_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TandemProcessor)
};
