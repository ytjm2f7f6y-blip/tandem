// Tandem: the whole instrument. Two layers (synthesizer modules), how the keys are shared between
// them (the play modes), an arpeggiator for each, the clock they run to, the external input's
// envelope follower, and the master volume.
#pragma once

#include "td_arp.hpp"
#include "td_layer.hpp"

namespace td {

class Engine {
public:
    static constexpr int kLayers = 2;
    static constexpr int kSpan = 1024;         // the most samples rendered in one go (the buffers' size)

    Engine() { for (auto& l : layer_) l.setUserWaves(&user_); }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    // Your own waves: the plugin publishes a table per slot (it keeps them alive).
    UserWaves& userWaves() { return user_; }

    void prepare(double rate, int maxBlock) {
        rate_ = rate;
        (void)maxBlock;
        for (int i = 0; i < kLayers; ++i) {
            layer_[size_t(i)].setSeed(uint32_t(i) * 7919u + 17u);
            layer_[size_t(i)].prepare(rate, maxBlock);
            arp_[size_t(i)].reset();
            arp_[size_t(i)].setSeed(uint32_t(i) + 1u);
        }
        volS_.setTime(0.02f, float(rate));
        volS_.reset(volume_);
        follow_.assign(size_t(kSpan), 0.0f);
        casL_.assign(size_t(kSpan), 0.0f);
        casR_.assign(size_t(kSpan), 0.0f);
        env_ = 0.0f;
        beat_ = origin_ = 0.0;
        hostLocked_ = false;
        route_.fill({});
        down_.fill(false);
        keysDown_ = 0;
        rotate_ = 0;
        rng_ = Rng(0xC0FFEEu);
    }
    double latencySamples() const { return layer_[0].latencySamples(); }

    Layer& layer(int i) { return layer_[size_t(i)]; }
    const Layer& layer(int i) const { return layer_[size_t(i)]; }
    const Arp& arp(int i) const { return arp_[size_t(i)]; }
    void setVolume(float v) { volume_ = clampf(v, 0.0f, 1.0f); }

    void setPerf(const PerfParams& p) { perf_ = p; }
    const PerfParams& perf() const { return perf_; }

    // The host's transport, once a block (before rendering it). With HOST on, the arpeggiators and
    // every synced time follow the host's tempo, and its beat while it plays.
    void setTransport(bool playing, double ppq, double hostBpm) {
        if (hostBpm > 0.0) hostBpm_ = float(hostBpm);
        const bool lock = perf_.hostSync && playing;
        if (lock) {
            const bool jump = !hostLocked_ || std::abs(ppq - beat_) > 0.05;     // starting, a loop or a locate
            beat_ = ppq;
            origin_ = 0.0;
            hostLocked_ = true;
            if (jump) for (int i = 0; i < kLayers; ++i) arp_[size_t(i)].jumped(layer_[size_t(i)], clock());
        } else hostLocked_ = false;
        const float t = tempo();
        for (auto& l : layer_) l.setTempo(t);
    }
    // For tests and hosts that give no transport: the tempo synced times use.
    void setTempo(float bpm) { hostBpm_ = bpm > 0.0f ? bpm : 120.0f; for (auto& l : layer_) l.setTempo(tempo()); }
    float tempo() const { return perf_.hostSync ? hostBpm_ : clampf(perf_.tempo, 20.0f, 300.0f); }
    double beat() const { return beat_; }

    // An external input is connected (the host's side chain): VCA ON/EXT follows it.
    void setExtConnected(bool on) { extOn_ = on; }
    float follower() const { return followNow_; }

    // ---- MIDI. Notes go through the play mode to the layers, then each layer's arpeggiator.
    void noteOn(int note, float vel, int channel = 0) {
        note = std::clamp(note, 0, 127);
        const size_t ki = keyIndex(note, channel);
        if (down_[ki]) noteOff(note, channel);                     // the same key again without letting go
        const int elig = eligible(note, channel), own = ownChannel(note, channel);
        int want = -1, semis = 0, mask = 0;
        switch (perf_.playMode) {
            case kPlayDual: case kPlayCascade: mask = elig; break;
            case kPlaySplit: want = note >= perf_.split ? 0 : 1; break;
            case kPlayOneTwo: want = keysDown_ == 0 ? 0 : 1; break;
            case kPlayRotate: want = rotate_; rotate_ ^= 1; break;
            case kPlayRandom: want = randomLayer(); break;
            case kPlayChaos: want = randomLayer(); semis = chaosSemis(); break;
            default: mask = elig & (1 << std::clamp(perf_.layer, 0, 1)); break;     // SINGLE: the layer being edited
        }
        if (want >= 0) mask = (elig >> want & 1) ? 1 << want : elig & ~(1 << want);   // not its keys: the other layer, if they are
        if (own && own != elig) mask = own;                // a layer set to this very channel: the note is that layer's
        down_[ki] = true;
        ++keysDown_;
        route_[ki] = {uint8_t(mask), int8_t(semis)};
        lastMask_ = mask;
        const int played = std::clamp(note + semis, 0, 127);
        for (int i = 0; i < kLayers; ++i) {
            if (!(mask >> i & 1)) continue;
            Arp& a = arp_[size_t(i)];
            if (a.on() && !a.running() && !hostLocked_ && !anyArpRunning()) origin_ = beat_;   // the grid starts with the first key
            a.keyOn(layer_[size_t(i)], played, vel, clock());
        }
    }
    void noteOff(int note, int channel = 0) {
        note = std::clamp(note, 0, 127);
        const size_t ki = keyIndex(note, channel);
        if (!down_[ki]) return;
        const Route r = route_[ki];
        down_[ki] = false;
        keysDown_ = std::max(0, keysDown_ - 1);
        route_[ki] = {};
        const int played = std::clamp(note + r.semis, 0, 127);
        for (int i = 0; i < kLayers; ++i)
            if (r.mask >> i & 1) arp_[size_t(i)].keyOff(layer_[size_t(i)], played);
    }
    // Which layers the last note went to (bit 0: layer 1), for the window.
    int lastRoute() const { return lastMask_; }
    // Whether a layer takes a note on this channel and key.
    bool hears(int layer, int note, int channel) const { return eligible(note, channel) >> layer & 1; }

    void sustain(bool on) { for (int i = 0; i < kLayers; ++i) arp_[size_t(i)].pedal(layer_[size_t(i)], on); }
    void bend(float v) { for (auto& l : layer_) l.bend(v); }
    void setTune(float semis) { for (auto& l : layer_) l.setTune(semis); }
    void modWheel(float v) { for (auto& l : layer_) l.modWheel(v); }
    void aftertouch(float v) { for (auto& l : layer_) l.aftertouch(v); }
    void expression(float v) { for (auto& l : layer_) l.expression(v); }
    void allNotesOff(bool hard = false) {
        for (int i = 0; i < kLayers; ++i) { arp_[size_t(i)].allOff(layer_[size_t(i)]); layer_[size_t(i)].allNotesOff(hard); }
        route_.fill({});
        down_.fill(false);
        keysDown_ = 0;
    }

    // Writes (not adds) n samples. xL/xR: the external input (the side chain), or null.
    void render(float* L, float* R, int n, const float* xL = nullptr, const float* xR = nullptr) {
        for (int i = 0; i < kLayers; ++i) arp_[size_t(i)].update(layer_[size_t(i)], layer_[size_t(i)].params(), clock());
        int done = 0;
        while (done < n) {
            const int m = std::min(kSpan, n - done);
            renderSpan(L + done, R + done, m, xL ? xL + done : nullptr, xR ? xR + done : (xL ? xL + done : nullptr));
            done += m;
        }
        for (int i = 0; i < n; ++i) {
            const float v = volS_.tick(volume_);
            const float g = v * v * 1.5f;
            L[i] *= g;
            R[i] *= g;
        }
    }

private:
    struct Route { uint8_t mask = 0; int8_t semis = 0; };

    Arp::Clock clock() const { return {beat_, origin_, hostLocked_}; }
    static size_t keyIndex(int note, int channel) { return size_t(channel > 0 ? ((channel - 1) & 15) : 0) * 128 + size_t(note); }
    bool anyArpRunning() const { for (const auto& a : arp_) if (a.running()) return true; return false; }

    int eligible(int note, int channel) const {
        int m = 0;
        for (int i = 0; i < kLayers; ++i) {
            const int ch = perf_.chan[i];
            if (ch > 0 && channel > 0 && ch != channel) continue;
            if (note < perf_.lo[i] || note > perf_.hi[i]) continue;
            m |= 1 << i;
        }
        return m;
    }
    // The layers set to listen on exactly this channel (not on all), that take this key.
    int ownChannel(int note, int channel) const {
        int m = 0;
        for (int i = 0; i < kLayers; ++i)
            if (channel > 0 && perf_.chan[i] == channel && note >= perf_.lo[i] && note <= perf_.hi[i]) m |= 1 << i;
        return m;
    }
    int randomLayer() { return rng_.uni() * 6.0f < float(std::clamp(perf_.layerOdds, 0, 6)) ? 0 : 1; }
    // CHAOS: now and then a fifth, now and then an octave, up or down.
    int chaosSemis() {
        int s = 0;
        auto roll = [&](int up, int down, int interval) {
            if (rng_.uni() * 6.0f < float(std::clamp(up, 0, 6))) s += interval;
            else if (rng_.uni() * 6.0f < float(std::clamp(down, 0, 6))) s -= interval;
        };
        roll(perf_.fifthUp, perf_.fifthDown, 7);
        roll(perf_.octUp, perf_.octDown, 12);
        return s;
    }

    // n <= kSpan: the follower over the span, then the layers between the arpeggiators' events.
    void renderSpan(float* L, float* R, int n, const float* xL, const float* xR) {
        std::fill(L, L + n, 0.0f);
        std::fill(R, R + n, 0.0f);
        float* f = follow_.data();
        if (xL) {
            const float atk = 1.0f - std::exp(-1.0f / (std::max(0.0005f, perf_.efAttack) * float(rate_)));
            const float rel = 1.0f - std::exp(-1.0f / (std::max(0.005f, perf_.efRelease) * float(rate_)));
            const float th = clampf(perf_.efThresh, 0.0f, 0.95f);
            for (int i = 0; i < n; ++i) {
                const float x = std::max(std::abs(xL[i]), std::abs(xR[i])) * perf_.efGain;
                env_ += (x - env_) * (x > env_ ? atk : rel);
                f[i] = clampf((env_ - th) / (1.0f - th), 0.0f, 1.0f);
            }
        } else {
            env_ = 0.0f;
            std::fill(f, f + n, 0.0f);
        }
        followNow_ = f[n - 1];
        const double bps = double(tempo()) / 60.0 / rate_;
        int done = 0;
        while (done < n) {
            for (int i = 0; i < kLayers; ++i) arp_[size_t(i)].fire(layer_[size_t(i)], clock());
            int m = n - done;
            for (const auto& a : arp_) {
                const double nb = a.nextBeat();
                if (nb < Arp::kNever) m = std::min(m, std::max(1, int(std::ceil((nb - beat_) / bps - 1e-6))));
            }
            ExtIn e1{xL ? xL + done : nullptr, xR ? xR + done : nullptr, f + done, extOn_};
            if (perf_.playMode == kPlayCascade) {             // layer 1 is layer 2's oscillator, heard only through it
                std::fill(casL_.begin(), casL_.begin() + m, 0.0f);
                std::fill(casR_.begin(), casR_.begin() + m, 0.0f);
                layer_[0].render(casL_.data(), casR_.data(), m, &e1);
                ExtIn e2{casL_.data(), casR_.data(), f + done, extOn_};
                layer_[1].render(L + done, R + done, m, &e2);
            } else {
                layer_[0].render(L + done, R + done, m, &e1);
                ExtIn e2{nullptr, nullptr, f + done, extOn_};
                layer_[1].render(L + done, R + done, m, &e2);
            }
            beat_ += double(m) * bps;
            done += m;
        }
    }

    UserWaves user_;
    std::array<Layer, kLayers> layer_;
    std::array<Arp, kLayers> arp_;
    PerfParams perf_;
    double rate_ = 44100.0;
    float volume_ = 0.8f;
    Smooth volS_;
    // the clock
    double beat_ = 0.0, origin_ = 0.0;
    float hostBpm_ = 120.0f;
    bool hostLocked_ = false;
    // the keys
    std::array<Route, 16 * 128> route_{};             // per channel and key: where each held key went
    std::array<bool, 16 * 128> down_{};
    int keysDown_ = 0, rotate_ = 0, lastMask_ = 0;
    Rng rng_{0xC0FFEEu};
    // the external input
    bool extOn_ = false;
    float env_ = 0.0f, followNow_ = 0.0f;
    std::vector<float> follow_, casL_, casR_;
};

}  // namespace td
