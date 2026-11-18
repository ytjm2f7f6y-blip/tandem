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
    // A sound is a patch (one layer: it can go on either) or a performance (both layers and the play mode).
    struct PresetEntry { juce::String name; bool factory = true; juce::File file; juce::String category; bool bothLayers = false; };
    std::vector<PresetEntry> presetList() const;
    void loadPreset(int index);                      // the whole instrument
    void stepPreset(int delta);
    juce::String presetName() const { return presetName_; }
    int presetIndex() const { return presetIndex_; }
    bool presetEdited() const;
    juce::String saveUserPreset(const juce::String& name);          // both layers, as a performance

    // ---- each layer's sound: any patch on either layer; from a performance, either of its layers
    void loadIntoLayer(int index, int layer, int part = 0);
    void stepLayer(int layer, int delta);            // the next or previous patch, on that layer
    std::vector<int> layerPatches() const;           // the one-layer sounds, in list order (indices into presetList())
    juce::String layerName(int layer) const { return layerName_[size_t(layer & 1)]; }
    int layerIndex(int layer) const { return layerIndex_[size_t(layer & 1)]; }
    int layerPart(int layer) const { return layerPart_[size_t(layer & 1)]; }       // which layer of a two-layer sound it came from
    bool layerEdited(int layer) const;
    juce::String saveLayerPatch(const juce::String& name, int layer);
    bool deleteUserPreset(int index);
    static juce::File presetFolder();

    // ---- compare
    int abSlot() const { return abSlot_; }
    void selectAB(int slot);

    // ---- your own waves (message thread): 8 slots, after the 32 built-in alternatives in every wave list.
    // A slot keeps one cycle (2048 points); it is saved with the session, and with any sound that uses it.
    static constexpr int kCycle = 2048;
    juce::String importWave(int slot, const juce::File& file);      // "" when it worked, else why not
    void setUserWave(int slot, const juce::String& name, const std::vector<float>& cycle);
    void clearUserWave(int slot);
    juce::String userWaveName(int slot) const { return slot >= 0 && slot < td::kUserWaves ? userName_[size_t(slot)] : juce::String(); }
    int freeUserWave() const;                                        // the first empty slot, or -1
    const std::vector<float>& userCycle(int slot) const { return userCycle_[size_t(juce::jlimit(0, td::kUserWaves - 1, slot))]; }

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
    static BusesProperties buses();          // the output; with TD_SIDECHAIN, the side chain input too
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
    // What is on the layers, by name: kept with each undo step and each of A and B.
    struct LayerNames { std::array<juce::String, 2> name{{"init", "init"}}; std::array<int, 2> index{{0, 0}}, part{{0, 0}}; std::array<Setting, 2> snap; };
    struct Snapshot { Setting values, snap; juce::String name; int index = 0; LayerNames layers; };
    struct UndoStep { juce::String what; Snapshot before, after; double when = 0; };
    Snapshot snapshot() const { return {currentSetting(), presetSnap_, presetName_, presetIndex_, layerNames()}; }
    LayerNames layerNames() const;
    void setLayerNames(const LayerNames& n);
    Setting layerValues(int layer) const;
    void nameWhole();                                // the performance's name, from its layers'
    std::array<juce::String, 2> layerName_{{"init", "init"}};
    std::array<int, 2> layerIndex_{{0, 0}}, layerPart_{{0, 0}};
    std::array<Setting, 2> layerSnap_;
    LayerNames abLayers_[2];
    void restore(const Snapshot& s);
    std::vector<UndoStep> undo_, redo_;
    Snapshot pending_;
    juce::String pendingWhat_;
    bool editing_ = false;

    std::array<std::unique_ptr<td::WaveTable>, td::kUserWaves> userTable_;
    std::vector<std::unique_ptr<td::WaveTable>> retired_;          // replaced tables, kept a while: the audio thread may still read one
    std::array<juce::String, td::kUserWaves> userName_;
    std::array<std::vector<float>, td::kUserWaves> userCycle_;
    // Your waves travel with what uses them. Reading puts each where it fits: the same wave already here is shared,
    // and a slot `keepLayer` uses is never overwritten (the wave moves to another slot). Returns file slot -> slot here (-1: no room).
    void writeWaves(juce::XmlElement& into, bool onlyUsed, int layer = -1) const;
    std::array<int, td::kUserWaves> readWaves(const juce::XmlElement& from, int keepLayer = -1);
    static float mapWave(float altValue, const std::array<int, td::kUserWaves>& map);
    bool waveUsed(int slot, int layer = -1) const;

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
