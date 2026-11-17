// Tandem: the whole instrument. Two layers (synthesizer modules) and the master volume.
// Stage 1 plays layer 1 only; the play modes that give notes to both arrive in stage 2.
#pragma once

#include "td_layer.hpp"

namespace td {

class Engine {
public:
    static constexpr int kLayers = 2;

    void prepare(double rate, int maxBlock) {
        rate_ = rate;
        for (int i = 0; i < kLayers; ++i) {
            layer_[size_t(i)].setSeed(uint32_t(i) * 7919u + 17u);
            layer_[size_t(i)].prepare(rate, maxBlock);
        }
        volS_.setTime(0.02f, float(rate));
        volS_.reset(volume_);
    }
    double latencySamples() const { return layer_[0].latencySamples(); }

    Layer& layer(int i) { return layer_[size_t(i)]; }
    const Layer& layer(int i) const { return layer_[size_t(i)]; }
    void setLayerOn(int i, bool on) { if (layerOn_[size_t(i)] != on) { layerOn_[size_t(i)] = on; if (!on) layer_[size_t(i)].allNotesOff(true); } }
    void setVolume(float v) { volume_ = clampf(v, 0.0f, 1.0f); }
    void setTempo(float bpm) { for (auto& l : layer_) l.setTempo(bpm); }

    // MIDI (stage 1: everything goes to layer 1).
    void noteOn(int note, float vel) { for (int i = 0; i < kLayers; ++i) if (layerOn_[size_t(i)]) layer_[size_t(i)].noteOn(note, vel); }
    void noteOff(int note) { for (int i = 0; i < kLayers; ++i) if (layerOn_[size_t(i)]) layer_[size_t(i)].noteOff(note); }
    void sustain(bool on) { for (auto& l : layer_) l.sustain(on); }
    void bend(float v) { for (auto& l : layer_) l.bend(v); }
    void modWheel(float v) { for (auto& l : layer_) l.modWheel(v); }
    void aftertouch(float v) { for (auto& l : layer_) l.aftertouch(v); }
    void expression(float v) { for (auto& l : layer_) l.expression(v); }
    void allNotesOff(bool hard = false) { for (auto& l : layer_) l.allNotesOff(hard); }

    // Writes (not adds) n samples.
    void render(float* L, float* R, int n) {
        std::fill(L, L + n, 0.0f);
        std::fill(R, R + n, 0.0f);
        for (int i = 0; i < kLayers; ++i) if (layerOn_[size_t(i)]) layer_[size_t(i)].render(L, R, n);
        for (int i = 0; i < n; ++i) {
            const float v = volS_.tick(volume_);
            const float g = v * v * 1.5f;
            L[i] *= g;
            R[i] *= g;
        }
    }

private:
    std::array<Layer, kLayers> layer_;
    std::array<bool, kLayers> layerOn_{{true, false}};
    double rate_ = 44100.0;
    float volume_ = 0.8f;
    Smooth volS_;
};

}  // namespace td
