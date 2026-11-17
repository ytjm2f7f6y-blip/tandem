// Tandem: one layer (one synthesizer module): its voices, how notes are given to them,
// and its effects.
#pragma once

#include "td_fx.hpp"
#include "td_voice.hpp"

#include <array>

namespace td {

class Layer {
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kChunk = 64;          // host samples per inner block

    void prepare(double hostRate, int maxBlock) {
        host_ = float(hostRate);
        os_ = hostRate < 70000.0 ? 2 : 1;
        fs_ = host_ * float(os_);
        (void)maxBlock;
        for (int i = 0; i < kMaxVoices; ++i) { voices_[size_t(i)].init(uint32_t(i + 1) * 977u + seed_); voices_[size_t(i)].active = false; }
        for (auto& d : dec_) d.reset();
        drive_.prepare(fs_);
        chorus_.prepare(host_);
        eq_.prepare(host_);
        delay_.prepare(host_);
        reverb_.prepare(host_);
        lfoShared_ = LfoCore{};
        lfoSharedR_ = LfoCore{};
        lfoSharedR_.rng = Rng(seed_ * 3u + 11u);
        heldCount_ = 0;
        clock_ = 1;
        levelS_.setTime(0.02f, host_); levelS_.reset(p_.level);
        panS_.setTime(0.02f, host_); panS_.reset(p_.pan);
        sendS_.setTime(0.02f, host_); sendS_.reset(p_.fxSend);
        configure();
    }
    void setSeed(uint32_t s) { seed_ = s; }
    // The latency the layer adds, in host samples (the decimator's).
    double latencySamples() const { return os_ == 2 ? 23.5 : 0.0; }
    float internalRate() const { return fs_; }

    void setParams(const LayerParams& p) {
        const bool binChange = (p.route == kRouteStereo) != (p_.route == kRouteStereo);
        const bool countChange = p.voices != p_.voices || p.voiceMode != p_.voiceMode;
        p_ = p;
        if (binChange || countChange) allNotesOff(true);
        configure();
    }
    const LayerParams& params() const { return p_; }

    void setTempo(float bpm) { bpm_ = bpm > 0.0f ? bpm : 120.0f; }
    void bend(float v) { bend_ = clampf(v, -1.0f, 1.0f); }
    void setTune(float semis) { tune_ = clampf(semis, -24.0f, 24.0f); }      // master tune and transpose
    void modWheel(float v) { mw_ = clampf(v, 0.0f, 1.0f); }
    void aftertouch(float v) { at_ = clampf(v, 0.0f, 1.0f); }
    void expression(float v) { expr_ = clampf(v, 0.0f, 1.0f); }
    bool overdriven() const { return odrive_; }
    int soundingVoices() const { int n = 0; for (int i = 0; i < nVoices_; ++i) n += voices_[size_t(i)].active; return n; }

    void sustain(bool on) {
        sustain_ = on;
        if (!on) {
            for (auto& v : voices_) if (v.active && v.sustained) { v.sustained = false; v.release(clock_++); }
            // Mono modes: notes let go of while held by the pedal leave the stack now.
            int w = 0;
            for (int i = 0; i < heldCount_; ++i) if (!heldSus_[size_t(i)]) { held_[size_t(w)] = held_[size_t(i)]; heldVel_[size_t(w)] = heldVel_[size_t(i)]; heldSus_[size_t(w)] = false; ++w; }
            heldCount_ = w;
            if (mono() && heldCount_ == 0) for (auto& v : voices_) if (v.active && v.gate) v.release(clock_++);
        }
    }

    void noteOn(int note, float vel) {
        pushHeld(note, vel);
        if (mono()) { monoNote(note, vel, true); return; }
        // Poly 2: a note already sounding takes its own voice again (its release is cut short).
        int vi = -1;
        if (p_.voiceMode == kPoly2) {
            for (int i = 0; i < nVoices_; ++i) if (voices_[size_t(i)].active && voices_[size_t(i)].note == note) { vi = i; break; }
            if (vi < 0) { vi = rr_ % nVoices_; rr_ = (rr_ + 1) % nVoices_; }
        } else {
            vi = pickVoice();
        }
        Voice& v = voices_[size_t(vi)];
        v.uniSemis = 0.0f;
        v.pan = voicePan(vi);
        v.trigger(note, vel, true, clock_++, ctx_);
    }

    void noteOff(int note) {
        if (mono()) {
            const bool top = heldCount_ > 0 && held_[size_t(heldCount_ - 1)] == note;
            if (sustain_) { markHeldSustained(note); return; }
            removeHeld(note);
            if (heldCount_ == 0) { for (int i = 0; i < nVoices_; ++i) if (voices_[size_t(i)].gate) voices_[size_t(i)].release(clock_++); }
            else if (top) monoNote(held_[size_t(heldCount_ - 1)], heldVel_[size_t(heldCount_ - 1)], false);
            return;
        }
        removeHeld(note);
        for (int i = 0; i < nVoices_; ++i) {
            Voice& v = voices_[size_t(i)];
            if (v.active && v.gate && v.note == note) {
                if (sustain_) v.sustained = true;
                else v.release(clock_++);
            }
        }
    }

    void allNotesOff(bool hard = false) {
        heldCount_ = 0;
        for (auto& v : voices_) {
            if (hard) { v.active = false; v.gate = false; v.e1.kill(); v.e2.kill(); v.e3.kill(); }
            else if (v.active && v.gate) v.release(clock_++);
        }
    }

    // Adds n host samples of this layer's output to outL/outR.
    void render(float* outL, float* outR, int n) {
        int done = 0;
        while (done < n) {
            const int m = std::min(kChunk, n - done);
            renderChunk(outL + done, outR + done, m);
            done += m;
        }
    }

private:
    bool mono() const { return p_.voiceMode == kSolo || p_.voiceMode == kLegato || p_.voiceMode == kUnison; }

    float voicePan(int vi) const {
        if (ctx_.binaural) return 0.0f;
        return p_.panSpread * ((vi & 1) ? 1.0f : -1.0f) * (vi < 2 ? 1.0f : (vi < 4 ? 0.6f : 0.35f));
    }

    int pickVoice() {
        int best = -1;
        for (int i = 0; i < nVoices_; ++i) if (!voices_[size_t(i)].active) return i;
        uint64_t oldest = ~0ull;
        for (int i = 0; i < nVoices_; ++i) {             // the voice released longest ago
            const Voice& v = voices_[size_t(i)];
            if (!v.gate && v.relAge < oldest) { oldest = v.relAge; best = i; }
        }
        if (best >= 0) return best;
        oldest = ~0ull;
        for (int i = 0; i < nVoices_; ++i)               // otherwise the oldest note
            if (voices_[size_t(i)].age < oldest) { oldest = voices_[size_t(i)].age; best = i; }
        return best < 0 ? 0 : best;
    }

    // Solo, Legato and Unison: one note at a time, the last one played wins.
    void monoNote(int note, float vel, bool fresh) {
        const bool anyGate = [&] { for (int i = 0; i < nVoices_; ++i) if (voices_[size_t(i)].gate) return true; return false; }();
        const bool retrig = p_.voiceMode == kLegato ? !anyGate : (fresh || !anyGate);
        const int count = p_.voiceMode == kUnison ? nVoices_ : 1;
        for (int i = 0; i < count; ++i) {
            Voice& v = voices_[size_t(i)];
            v.uniSemis = p_.voiceMode == kUnison ? unisonOffset(i, count) : 0.0f;
            v.pan = p_.voiceMode == kUnison && !ctx_.binaural && count > 1 ? (float(i) / float(count - 1) * 2.0f - 1.0f) * std::max(0.6f, p_.panSpread) : voicePan(i);
            v.trigger(note, vel, retrig, clock_++, ctx_);
        }
    }
    float unisonOffset(int i, int count) const {
        switch (p_.unisonMode) {
            case kUniLevel1: return 0.0f;
            case kUniLevel2: return count > 1 ? (float(i) / float(count - 1) - 0.5f) * 0.24f : 0.0f;   // +-12 cents
            case kUniOctave: return (i & 1) ? 12.0f : 0.0f;
            case kUniFifth: return (i & 1) ? 7.0f : 0.0f;
            case kUniMajor: { static const float c[4] = {0.0f, 4.0f, 7.0f, 12.0f}; return c[i % 4] + (i >= 4 ? 12.0f : 0.0f); }
            default: return 0.0f;
        }
    }

    void pushHeld(int note, float vel) {
        removeHeld(note);
        if (heldCount_ >= int(held_.size())) { for (int i = 1; i < heldCount_; ++i) { held_[size_t(i - 1)] = held_[size_t(i)]; heldVel_[size_t(i - 1)] = heldVel_[size_t(i)]; heldSus_[size_t(i - 1)] = heldSus_[size_t(i)]; } --heldCount_; }
        held_[size_t(heldCount_)] = note; heldVel_[size_t(heldCount_)] = vel; heldSus_[size_t(heldCount_)] = false;
        ++heldCount_;
    }
    void removeHeld(int note) {
        int w = 0;
        for (int i = 0; i < heldCount_; ++i) if (held_[size_t(i)] != note) { held_[size_t(w)] = held_[size_t(i)]; heldVel_[size_t(w)] = heldVel_[size_t(i)]; heldSus_[size_t(w)] = heldSus_[size_t(i)]; ++w; }
        heldCount_ = w;
    }
    void markHeldSustained(int note) { for (int i = 0; i < heldCount_; ++i) if (held_[size_t(i)] == note) heldSus_[size_t(i)] = true; }

    // Works out everything the voices share for the next blocks.
    void configure() {
        const LayerParams& p = p_;
        BlockCtx& c = ctx_;
        c.p = &p_;
        c.fs = fs_;
        c.invFs = 1.0f / fs_;
        c.binaural = p.route == kRouteStereo;
        nVoices_ = std::clamp(c.binaural ? p.voices / 2 : p.voices, 1, kMaxVoices);
        for (int i = 0; i < kMaxVoices; ++i) voices_[size_t(i)].setEars(c.binaural);
        const auto& bank = WaveBank::get();
        auto oscCfg = [&](OscCfg& oc, int wave, int osc, float tune, float fine) {
            oc.drone = false;
            oc.tuneSemis = tune + fine * 0.01f;
            oc.pwBase = 0.5f; oc.pwSpan = 0.46f;
            const int altBase = kWAlt1;
            const int nAlt = osc == 0 ? 6 : 3;
            const int* alt = osc == 0 ? p.osc1Alt : p.osc2Alt;
            int w = wave;
            if (osc == 1 && w >= kWDroneSquare) { oc.drone = true; w = w == kWDroneSquare ? kWSquare : (w == kWDroneSaw ? kWSaw : kWSine); }
            switch (w) {
                case kWSine: oc.kind = 0; oc.a = &bank.classic(WaveBank::kSine); oc.b = &bank.classic(WaveBank::kSaw); break;
                case kWSaw: oc.a = &bank.classic(WaveBank::kSaw); oc.b = nullptr; break;
                case kWSquare: oc.kind = 1; oc.a = &bank.classic(WaveBank::kSaw); oc.pwBase = 0.5f; oc.pwSpan = 0.46f; break;
                case kWPulse: oc.kind = 1; oc.a = &bank.classic(WaveBank::kSaw); oc.pwBase = 0.25f; oc.pwSpan = 0.21f; break;
                case kWTri: oc.kind = 0; oc.a = &bank.classic(WaveBank::kTri); oc.b = &bank.classic(WaveBank::kSaw); break;
                case kWNoise: oc.kind = 2; oc.a = oc.b = nullptr; break;
                default: {
                    const int slot = std::clamp(w - altBase, 0, nAlt - 1);
                    oc.kind = 0;
                    oc.a = &bank.alt(alt[slot]);
                    oc.b = &bank.alt(alt[(slot + 1) % nAlt]);          // wave mod scans to the next slot
                }
            }
            if (w == kWSaw) oc.kind = 3;                              // a saw morphs to a square: saw(p) - m saw(p + 1/2)
            if (oc.b == nullptr && oc.kind == 0) oc.b = oc.a;
            const bool modThis = p.modOsc == 1 || p.modOsc == osc * 2;
            oc.detuned = modThis && p.detune > 0.0f && oc.kind != 2;
            oc.waveMod = modThis && p.waveMod > 0.0f;
            oc.pitchMod = modThis && p.pitchSrc != 2;
        };
        oscCfg(c.osc[0], p.osc1Wave, 0, p.osc1Tune, p.osc1Fine);
        oscCfg(c.osc[1], p.osc2Wave, 1, p.osc2Tune, p.osc2Fine);

        // The super oscillator: how far the sisters spread (cents) and how loud they are.
        const float D = clampf(p.detune, 0.0f, 1.0f);
        c.detuneCents = 70.0f * std::pow(D, 1.8f);
        c.tierW[0] = 1.0f - 0.3f * D;
        const float sd = std::sqrt(D);
        c.tierW[1] = 0.62f * sd; c.tierW[2] = 0.59f * sd; c.tierW[3] = 0.56f * sd;
        c.earGain = c.binaural ? 1.0f / (1.0f + 0.25f * D) : 1.0f / (1.0f + 0.6f * D);

        // Filter.
        c.route = p.route;
        int ka = 0, kb = 0;
        bool lpType = false;
        if (p.route == kRouteStereo) {
            ka = std::clamp(p.stereoType, 0, kNumStereoTypes - 1);
            lpType = ka <= kS1LP;
        } else if (p.route == kRouteParallel) {
            static const int A[kNumParallelTypes] = {1, 2, 7, 6};
            ka = kb = A[std::clamp(p.parallelType, 0, kNumParallelTypes - 1)];
            lpType = p.parallelType <= kP1LP1LP;
        } else {
            static const int A[kNumSeriesTypes] = {9, 9, 9, 9, 2, 2}, B[kNumSeriesTypes] = {2, 1, 5, 9, 2, 5};
            const int t = std::clamp(p.seriesType, 0, kNumSeriesTypes - 1);
            ka = A[t]; kb = B[t];
            lpType = t == kQ1LP1LP;
        }
        c.mixA = poleMix(ka);
        c.mixB = poleMix(kb);
        c.lfComp = (lpType && p.resType == 0) ? 1.0f : 0.0f;
        c.cutOct = clampf(p.cutoff, 0.0f, 1.0f) * 10.3f;
        c.kRes = 4.3f * std::pow(clampf(p.resonance, 0.0f, 1.0f), 1.2f);
        c.g1 = 2.2f * p.osc1Level * p.osc1Level;
        c.g2 = 2.2f * p.osc2Level * p.osc2Level;

        // Envelopes.
        c.e1T[0] = Env::holdTime(p.e1Hold); c.e1T[1] = Env::attackTime(p.e1A); c.e1T[2] = Env::decayTime(p.e1D); c.e1T[3] = p.e1S; c.e1T[4] = Env::decayTime(p.e1R);
        c.e2T[0] = Env::attackTime(p.e2A); c.e2T[1] = Env::decayTime(p.e2D); c.e2T[2] = p.e2S; c.e2T[3] = Env::decayTime(p.e2R);
        c.e3T[0] = Env::holdTime(p.e3Hold); c.e3T[1] = Env::attackTime(p.e3A); c.e3T[2] = Env::decayTime(p.e3D); c.e3T[3] = p.e3S; c.e3T[4] = Env::decayTime(p.e3R);

        // LFO 1.
        c.lfo1HF = p.lfo1Wave >= kLHF;
        c.lfo1Shared = !c.lfo1HF && p.lfo1Mode == 0;
        if (p.lfo1Wave == kLHF) c.lfo1Hz = 20.0f * std::pow(1000.0f, p.lfo1Rate);
        else if (p.lfo1Wave == kLHFTrack) c.lfo1Hz = exp2Fast((p.lfo1Rate - 0.5f) * 4.0f);              // +-2 octaves from the note
        else if (p.lfo1Sync) {
            const int d = std::clamp(int(p.lfo1Rate * (kNumLfoDivs - 1) + 0.5f), 0, kNumLfoDivs - 1);
            c.lfo1Hz = bpm_ / 60.0f / kLfoBeats[d];
        } else c.lfo1Hz = 0.03f * std::pow(40.0f / 0.03f, p.lfo1Rate);
        c.lfoUser[0] = bank.alt(p.lfoAlt[0]).level(0);
        c.lfoUser[1] = bank.alt(p.lfoAlt[1]).level(0);
        c.lfo1DelayS = 5.0f * p.lfo1Delay * p.lfo1Delay * p.lfo1Delay;
        c.lfo2DelayS = 5.0f * p.lfo2Delay * p.lfo2Delay * p.lfo2Delay;
        c.lfo2Hz = 0.05f * std::pow(600.0f, p.lfo2Rate);
        c.driftAmt = p.drift;
        c.glideRate = p.portamento <= 0.001f ? 0.0f : 12.0f / (10.0f * std::pow(p.portamento, 2.5f));
        c.bpm = bpm_;
    }

    void renderChunk(float* outL, float* outR, int n) {
        configureLive();
        const int N = n * os_;
        float* bl = bufL_.data();
        float* br = bufR_.data();
        std::fill(bl, bl + N, 0.0f);
        std::fill(br, br + N, 0.0f);
        ctx_.n = N;
        ctx_.odrive = false;

        // The shared LFO 1 (free running, low-frequency modes), one value per sample.
        if (ctx_.lfo1Shared) {
            const Voice* newest = newestVoice();
            const float rateK = newest ? exp2Fast(4.0f * newest->modOut[kMdLfo1Rate]) : 1.0f;
            const uint32_t inc = uint32_t(clampf(ctx_.lfo1Hz * rateK * ctx_.invFs, 0.0f, 0.25f) * 4294967296.0);
            const uint32_t offs = uint32_t(double(p_.lfo1Phase) * 4294967295.0);
            const bool white = p_.lfo1Wave == kLSampleHold && p_.lfo1Rate > 0.985f && !p_.lfo1Sync;
            const float* user = ctx_.lfoUser[p_.lfo1Wave == kLUser2 ? 1 : 0];
            for (int i = 0; i < N; ++i) {
                lfoL_[size_t(i)] = lfoShared_.tick(p_.lfo1Wave, inc, ctx_.invFs, user, false, white);
                lfoSharedR_.ph = lfoShared_.ph + offs;
                if (lfoShared_.ph < inc) lfoSharedR_.hold = lfoSharedR_.rng.bi();
                lfoR_[size_t(i)] = ctx_.binaural ? lfoSharedR_.shape(p_.lfo1Wave, lfoSharedR_.ph, user, white) : lfoL_[size_t(i)];
            }
            ctx_.sharedL = lfoL_.data();
            ctx_.sharedR = lfoR_.data();
        }

        for (int i = 0; i < nVoices_; ++i) {
            Voice& v = voices_[size_t(i)];
            if (!v.active) continue;
            v.render(ctx_, bl, br, nullptr);
            if (v.finished(p_)) v.active = false;
        }
        odrive_ = ctx_.odrive;

        drive_.process(p_, bl, br, N);

        // Down to the host's rate.
        float* hl = hostL_.data();
        float* hr = hostR_.data();
        if (os_ == 2) {
            for (int i = 0; i < n; ++i) {
                hl[i] = dec_[0].process(bl[2 * i], bl[2 * i + 1]);
                hr[i] = dec_[1].process(br[2 * i], br[2 * i + 1]);
            }
        } else {
            std::copy(bl, bl + n, hl);
            std::copy(br, br + n, hr);
        }

        chorus_.process(p_, hl, hr, n);
        eq_.process(p_, hl, hr, n);

        // Level and pan (constant power), then the sends.
        float* sl = sendL_.data();
        float* sr = sendR_.data();
        for (int i = 0; i < n; ++i) {
            const float lv = levelS_.tick(p_.level);
            const float pn = panS_.tick(p_.pan);
            const float sd = sendS_.tick(p_.fxSend);
            const float g = 1.6f * lv * lv;
            const float gl = g * std::sqrt(0.5f * (1.0f - pn)) * 1.41421356f, gr = g * std::sqrt(0.5f * (1.0f + pn)) * 1.41421356f;
            const float l = hl[i] * gl, r = hr[i] * gr;
            const float dryK = 1.0f - 0.35f * sd;
            outL[i] += l * dryK;
            outR[i] += r * dryK;
            sl[i] = l * sd;
            sr[i] = r * sd;
        }
        const float balD = std::cos(p_.fxBal * 1.5707963f), balR = std::sin(p_.fxBal * 1.5707963f);
        float* dl = tmpL_.data();
        float* dr = tmpR_.data();
        std::fill(dl, dl + n, 0.0f);
        std::fill(dr, dr + n, 0.0f);
        const Voice* newest = newestVoice();
        const float timeMod = newest ? newest->modOut[kMdDelayTime] : 0.0f;
        delay_.process(p_, bpm_, timeMod, sl, sr, dl, dr, n);
        for (int i = 0; i < n; ++i) { outL[i] += dl[i] * balD; outR[i] += dr[i] * balD; }
        reverb_.process(p_, sl, sr, outL, outR, n, balR);
    }

    // Things that change while notes play (tempo for synced times).
    void configureLive() {
        if (ctx_.bpm != bpm_) configure();
        ctx_.bendSemis = bend_ * p_.bendRange + tune_;
        ctx_.bendVcfOct = bend_ * p_.bendVcf * 4.0f;
        ctx_.modWheel = mw_;
        ctx_.aftertouch = at_;
        ctx_.expression = expr_;
        const size_t need = size_t(kChunk * 2);
        if (bufL_.size() < need) {
            bufL_.assign(need, 0.0f); bufR_.assign(need, 0.0f);
            lfoL_.assign(need, 0.0f); lfoR_.assign(need, 0.0f);
            hostL_.assign(need, 0.0f); hostR_.assign(need, 0.0f);
            sendL_.assign(need, 0.0f); sendR_.assign(need, 0.0f);
            tmpL_.assign(need, 0.0f); tmpR_.assign(need, 0.0f);
        }
    }
    const Voice* newestVoice() const {
        const Voice* best = nullptr;
        for (int i = 0; i < nVoices_; ++i) if (voices_[size_t(i)].active && (!best || voices_[size_t(i)].age > best->age)) best = &voices_[size_t(i)];
        return best;
    }

    LayerParams p_;
    BlockCtx ctx_;
    std::array<Voice, kMaxVoices> voices_;
    int nVoices_ = 2, rr_ = 0;
    float host_ = 44100.0f, fs_ = 88200.0f;
    int os_ = 2;
    uint32_t seed_ = 1;
    uint64_t clock_ = 1;
    float bpm_ = 120.0f, bend_ = 0.0f, mw_ = 0.0f, at_ = 0.0f, expr_ = 0.0f, tune_ = 0.0f;
    bool sustain_ = false, odrive_ = false;
    std::array<int, 32> held_{};
    std::array<float, 32> heldVel_{};
    std::array<bool, 32> heldSus_{};
    int heldCount_ = 0;
    LfoCore lfoShared_, lfoSharedR_;
    Decimator2 dec_[2];
    Drive drive_;
    Chorus chorus_;
    Eq3 eq_;
    Delay delay_;
    Reverb reverb_;
    Smooth levelS_, panS_, sendS_;
    std::vector<float> bufL_, bufR_, lfoL_, lfoR_, hostL_, hostR_, sendL_, sendR_, tmpL_, tmpR_;
};

}  // namespace td
