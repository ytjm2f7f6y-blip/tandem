// Tandem: one voice of a layer, and the parts it is made of.
//
// A voice runs at the layer's internal rate (twice the host's below 70 kHz). In Binaural
// mode (the STEREO filter routing) it has two "ears", each a complete path of oscillators,
// filter and amplifier, panned hard left and right: two of the hardware's voices joined as
// one stereo super voice. Otherwise it has one ear and is panned by the voice spread.
#pragma once

#include "td_dsp.hpp"
#include "td_params.hpp"
#include "td_wave.hpp"

namespace td {

// ---------------------------------------------------------------------------------------
// Envelopes: analog-style RC curves. The attack aims past full scale (at 1.25) and stops at
// 1, so it rises nearly in a straight line; decay and release are exponential, and their
// time is the time to get 99% of the way. A retrigger starts the attack from wherever the
// envelope is, so a stolen voice never clicks.
struct Env {
    enum Stage { kIdle, kHold, kAttack, kDecay, kRelease };
    Stage st = kIdle;
    float v = 0.0f, holdLeft = 0.0f;
    float a = 1.0f, d = 1.0f, r = 1.0f, s = 0.0f, hold = 0.0f;
    bool loop = false;

    static float attackTime(float x) { return 0.0005f * std::pow(30000.0f, clampf(x, 0, 1)); }   // 0.5 ms .. 15 s
    static float decayTime(float x) { return 0.002f * std::pow(15000.0f, clampf(x, 0, 1)); }     // 2 ms .. 30 s
    static float holdTime(float x) { x = clampf(x, 0, 1); return 5.0f * x * x * x; }             // 0 .. 5 s

    void set(float A, float D, float S, float R, float H, bool lp, float fs) {
        a = 1.0f - std::exp(-1.6094379f / (std::max(A, 1e-5f) * fs));
        d = 1.0f - std::exp(-4.6051702f / (std::max(D, 1e-5f) * fs));
        r = 1.0f - std::exp(-4.6051702f / (std::max(R, 1e-5f) * fs));
        s = clampf(S, 0.0f, 1.0f);
        hold = H;
        loop = lp && s < 0.995f;
    }
    void gateOn() {
        holdLeft = hold;
        st = hold > 0.0f ? kHold : kAttack;
    }
    void gateOff() {
        if (st != kIdle) st = kRelease;
    }
    void kill() { st = kIdle; v = 0.0f; }
    bool idle() const { return st == kIdle; }
    float tick(float invFs) {
        switch (st) {
            case kIdle: break;
            case kHold:
                holdLeft -= invFs;
                if (holdLeft <= 0.0f) st = kAttack;
                break;
            case kAttack:
                v += (1.25f - v) * a;
                if (v >= 1.0f) { v = 1.0f; st = kDecay; }
                break;
            case kDecay:
                v += (s - v) * d;
                if (loop && v <= s + 0.01f * (1.0f - s)) st = kAttack;
                break;
            case kRelease:
                v -= v * r;
                if (v < 1e-5f) { v = 0.0f; st = kIdle; }
                break;
        }
        return v;
    }
};

// ---------------------------------------------------------------------------------------
// LFO shapes. Bipolar: sine, S&H, noise, triangle, noisy sine, user waves. Unipolar (0..1):
// sawtooth (falling), square, ramp (rising), as the hardware's manual draws them.
struct LfoCore {
    uint32_t ph = 0;
    float hold = 0.0f, noiseHeld = 0.0f, noiseSm = 0.0f, noiseClock = 0.0f;
    bool done = false;
    Rng rng;

    static float sine(uint32_t p) { return WaveTable::read(WaveBank::get().classic(WaveBank::kSine).level(0), p); }
    static float tri(uint32_t p) {
        const float f = float(p) * (1.0f / 4294967296.0f);
        return f < 0.25f ? 4.0f * f : (f < 0.75f ? 2.0f - 4.0f * f : 4.0f * f - 4.0f);
    }
    // Advance by inc (a 32-bit phase step); `white` turns S&H into white noise (the top of the RATE fader).
    float tick(int wave, uint32_t inc, float invFs, const float* user, bool once, bool white = false) {
        const uint32_t old = ph;
        if (!done) {
            ph += inc;
            if (ph < old) {                                   // wrapped
                if (once) { done = true; ph = 0xFFFFFFFFu; }
                hold = rng.bi();
            }
        }
        // The fixed-rate noise: a new value 25 times a second, smoothed.
        noiseClock += 25.0f * invFs;
        if (noiseClock >= 1.0f) { noiseClock -= 1.0f; noiseHeld = rng.bi(); }
        noiseSm += (noiseHeld - noiseSm) * std::min(1.0f, 250.0f * invFs);
        return shape(wave, ph, user, white);
    }
    float shape(int wave, uint32_t p, const float* user, bool white) {
        const float f = float(p) * (1.0f / 4294967296.0f);
        switch (wave) {
            case kLSine: return sine(p);
            case kLSaw: return 1.0f - f;
            case kLSampleHold: return white ? rng.bi() : hold;
            case kLSquare: return f < 0.5f ? 1.0f : 0.0f;
            case kLRamp: return f;
            case kLNoise: return noiseSm;
            case kLTri: return tri(p);
            case kLNoisySine: return 0.8f * sine(p) + 0.25f * rng.bi();
            case kLUser1: case kLUser2: return user ? WaveTable::read(user, p) : 0.0f;
            default: return sine(p);
        }
    }
};

inline float lfo2Shape(int wave, uint32_t p, float hold, float noise) {
    const float f = float(p) * (1.0f / 4294967296.0f);
    switch (wave) {
        case kL2Tri: return LfoCore::tri(p);
        case kL2Saw: return 1.0f - f;
        case kL2SampleHold: return hold;
        case kL2Square: return f < 0.5f ? 1.0f : 0.0f;
        case kL2Ramp: return f;
        case kL2Noise: return noise;
        default: return 0.0f;
    }
}

// ---------------------------------------------------------------------------------------
// The filter core: four OTA one-pole stages in a loop, solved without a unit delay
// (Zavalishin's topology-preserving transform). The loop's input stage saturates, which
// sets the level of self-oscillation and gives the overdrive. Outputs are mixed from the
// input and the four stage taps (pole mixing), which is how one core makes low, high and
// band pass, all pass and phaser responses with 1 to 4 poles.
// Resonance type 1 feeds back through a 2-pole high pass at a quarter of the cutoff (so
// what is fed back is a band round the cutoff), and the low end does not thin out as the
// resonance rises; type 2 feeds back everything, like the 4-pole filters of many vintage
// polysynths.
struct Mix5 { float c[5]; };                    // weights for u, y1, y2, y3, y4

inline Mix5 poleMix(int kind) {
    // kind: 0 4LP 1 2LP 2 1LP 3 3HP 4 2HP 5 1HP 6 4BP 7 2BP 8 4AP 9 4PS 10 1LP+3HP 11 3LP+1HP
    // L = 1 / (1 + s): HP^n = (1 - L)^n, BP peaks normalised to 1, AP4 = (2L - 1)^4, PS = (1 + AP4) / 2.
    switch (kind) {
        case 0: return {{0, 0, 0, 0, 1}};
        case 1: return {{0, 0, 1, 0, 0}};
        case 2: return {{0, 1, 0, 0, 0}};
        case 3: return {{1, -3, 3, -1, 0}};
        case 4: return {{1, -2, 1, 0, 0}};
        case 5: return {{1, -1, 0, 0, 0}};
        case 6: return {{0, 0, 4, -8, 4}};
        case 7: return {{0, 2, -2, 0, 0}};
        case 8: return {{1, -8, 24, -32, 16}};
        case 9: return {{1, -4, 12, -16, 8}};
        case 10: return {{0, 3.0792f, -9.2376f, 9.2376f, -3.0792f}};
        case 11: return {{0, 0, 0, 3.0792f, -3.0792f}};
        default: return {{0, 0, 0, 0, 1}};
    }
}

struct FilterCore {
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0, h1 = 0, h2 = 0;
    void reset() { s1 = s2 = s3 = s4 = h1 = h2 = 0; }
    // g = tan(pi fc / fs); k = feedback (4 = the edge of self-oscillation for type 2);
    // gh > 0 (type 1): the feedback goes through a 2-pole high pass, gh = tan(pi fh / fs).
    inline float process(float x, float g, float k, float gh, const Mix5& m) {
        const float G = g / (1.0f + g);
        const float inv = 1.0f / (1.0f + g);
        const float S1 = s1 * inv, S2 = s2 * inv, S3 = s3 * inv, S4 = s4 * inv;
        const float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        const float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;            // y4 = G^4 u + sigma
        float fc = 1.0f, fo = 0.0f, Gh = 0.0f;
        if (gh > 0.0f) {                                                 // feedback = c^2 y4 - (c Sa + Sb)
            Gh = gh / (1.0f + gh);
            const float c = 1.0f - Gh, ih = 1.0f / (1.0f + gh);
            fc = c * c;
            fo = c * h1 * ih + h2 * ih;
        }
        float u = (x - k * (fc * sigma - fo)) / (1.0f + k * fc * G4);
        u = kneeSat(u, 0.7f, 2.0f);                                      // the OTA input stage
        float v = (u - s1) * G; const float y1 = v + s1; s1 = y1 + v;
        v = (y1 - s2) * G; const float y2 = v + s2; s2 = y2 + v;
        v = (y2 - s3) * G; const float y3 = v + s3; s3 = y3 + v;
        v = (y3 - s4) * G; const float y4 = v + s4; s4 = y4 + v;
        if (gh > 0.0f) {
            v = (y4 - h1) * Gh; const float la = v + h1; h1 = la + v;
            const float ha = y4 - la;
            v = (ha - h2) * Gh; const float lb = v + h2; h2 = lb + v;
        }
        return m.c[0] * u + m.c[1] * y1 + m.c[2] * y2 + m.c[3] * y3 + m.c[4] * y4;
    }
};

// Type 1's high pass in the loop moves where the filter sings and how much feedback it
// needs; these put both back where type 2 has them (worked out from the loop's response).
constexpr float kType1Tune = 1.0f / 1.2247f, kType1Gain = 6.51f / 4.0f;

// ---------------------------------------------------------------------------------------
// Settings worked out once per block and shared by every voice of the layer.
struct OscCfg {
    int kind = 0;                       // 0 table (with morph), 1 pulse/square (two saws), 2 noise, 3 saw (morphs to square)
    const WaveTable* a = nullptr;       // the selected wave
    const WaveTable* b = nullptr;       // the wave it morphs towards
    float pwBase = 0.5f, pwSpan = 0.46f;
    bool drone = false;
    float tuneSemis = 0.0f;
    bool detuned = false, waveMod = false, pitchMod = false;
};

struct EarSet {                         // the super oscillator's sisters for one ear
    int n = 7;
    float cents[7] = {0, 1, -1, 0.62f, -0.62f, 0.21f, -0.21f};
    int tier[7] = {0, 1, 1, 2, 2, 3, 3};
};

struct BlockCtx {
    const LayerParams* p = nullptr;
    float fs = 88200.0f, invFs = 1.0f / 88200.0f;
    int n = 0;
    OscCfg osc[2];
    float detuneCents = 0.0f, tierW[4] = {1, 0, 0, 0}, earGain = 1.0f;   // sisters' spread and levels
    bool binaural = true;
    Mix5 mixA{}, mixB{};
    int route = 0;
    float lfComp = 0.0f;
    float cutOct = 0.0f, kRes = 0.0f;
    float g1 = 0.0f, g2 = 0.0f;         // mixer gains
    float e1T[5], e2T[4], e3T[5];       // times / levels (seconds, sustain)
    uint32_t lfo1Inc = 0;               // shared LFO 1 step (low-frequency modes)
    float lfo1Hz = 1.0f;
    bool lfo1Shared = false, lfo1HF = false;
    const float* lfoUser[2] = {nullptr, nullptr};
    const float* sharedL = nullptr;     // the shared LFO 1, per sample, left and right
    const float* sharedR = nullptr;
    float lfo1DelayS = 0.0f, lfo2DelayS = 0.0f, lfo2Hz = 1.0f;
    float bendSemis = 0.0f, bendVcfOct = 0.0f;
    float modWheel = 0.0f, aftertouch = 0.0f, expression = 0.0f, envFollow = 0.0f;
    float glideRate = 0.0f;             // semitones per second (0: off)
    // Audio from outside the voice into its filter (internal rate, this block): the EXT IN on layer 1, layer 1's
    // output on layer 2 in CASCADE. Out of the binaural routing extL carries both channels' sum.
    const float* extL = nullptr;
    const float* extR = nullptr;
    float gExt = 0.0f;
    const float* follow = nullptr;      // the envelope follower, per sample (internal rate), 0..1
    bool extVca = false;                // VCA ON/EXT with an input connected: the follower opens the VCA
    float driftAmt = 0.0f;
    float bpm = 120.0f;
    bool odrive = false;                // set by voices when the mixer is driven into the filter
};

// ---------------------------------------------------------------------------------------
struct Voice {
    // ---- note state
    int note = 60;
    float vel = 0.8f;
    bool gate = false, sustained = false, active = false;
    uint64_t age = 0;                   // when it was last triggered
    uint64_t relAge = 0;                // when it was last released
    double pitch = 60.0, target = 60.0;    // semitones; double, so slow glides' tiny steps are not lost
    float uniSemis = 0.0f, pan = 0.0f;
    // ---- drift: fixed offsets per voice plus a slow wander
    float dPitch = 0, dCut = 0, dEnv = 0, wander = 0, wanderTarget = 0, wanderClock = 0;
    Rng rng;

    Env e1, e2, e3;
    LfoCore lfo1L, lfo1R;
    float lfo1Fade = 1.0f, lfo2Fade = 1.0f, lfo1Time = 0.0f, lfo2Time = 0.0f;
    uint32_t lfo2Ph = 0;
    float lfo2Hold = 0.0f, lfo2Noise = 0.0f;

    struct Ear {
        uint32_t ph[2][7] = {};
        float last[2] = {0, 0};          // each oscillator's last output (cross mod, filter FM, matrix)
        float ring[2][BlepTable::kTaps] = {};   // 8 samples' delay, so sync's steps can be band-limited
        int rpos = 0;
        float noiseLp[2] = {0, 0};
        FilterCore fa, fb;
        uint32_t hfPh = 0;
        Rng rng;
        void reset() { fa.reset(); fb.reset(); std::memset(ring, 0, sizeof ring); last[0] = last[1] = 0; noiseLp[0] = noiseLp[1] = 0; }
    } ear[2];
    EarSet set[2];

    float modOut[kNumModDests] = {};     // the matrix's sum per destination (this sample)
    float slotZ[kMatrixSlots] = {};      // SMOOTH slots' slewed outputs
    bool slotFresh = true;               // a voice starting from silence: its SMOOTH slots start where they are
    // ---- set by the layer before each trigger
    int key = 60;                        // the key that started it (VARY may play it an octave away)
    float noteRnd = 0.0f;                // NOTE RND's value for this note, -1..1
    float varyCut = 0.0f, varyEnvK = 1.0f;   // VARY: octaves on the cutoff, a factor on the envelope times
    float meter = 0.0f;                  // how loud it was over the last block (its VCA), for the lamps

    void init(uint32_t seed) {
        rng = Rng(seed * 2654435761u + 12345u);
        ear[0].rng = Rng(seed * 7u + 1u);
        ear[1].rng = Rng(seed * 13u + 5u);
        lfo1L.rng = Rng(seed * 31u + 3u);
        lfo1R.rng = Rng(seed * 37u + 9u);
        dPitch = rng.bi(); dCut = rng.bi(); dEnv = rng.bi();
        for (auto& e : ear)
            for (auto& o : e.ph)
                for (auto& p : o) p = rng.next();
    }

    void setEars(bool binaural) {
        if (binaural) {
            const float cl[4] = {0, 1, -0.62f, 0.21f}, cr[4] = {0, -1, 0.62f, -0.21f};
            for (int i = 0; i < 4; ++i) { set[0].cents[i] = cl[i]; set[1].cents[i] = cr[i]; set[0].tier[i] = set[1].tier[i] = i; }
            set[0].n = set[1].n = 4;
        } else {
            set[0] = EarSet{};
            set[1] = EarSet{};
        }
    }

    void trigger(int nt, float velocity, bool retrigger, uint64_t when, const BlockCtx& c) {
        const bool wasIdle = !active;
        note = nt;
        vel = velocity;
        gate = true;
        sustained = false;
        active = true;
        age = when;
        target = double(nt);
        if (wasIdle || c.glideRate <= 0.0f) pitch = target;
        if (wasIdle) {
            ear[0].reset();
            ear[1].reset();
            for (auto& e : ear)                  // the sisters start at random phases; the centres together
                for (auto& o : e.ph) {
                    for (int i = 1; i < 7; ++i) o[i] = rng.next();
                }
            ear[1].ph[0][0] = ear[0].ph[0][0];
            ear[1].ph[1][0] = ear[0].ph[1][0];
            e1.kill(); e2.kill(); e3.kill();
            slotFresh = true;
        }
        dPitch = 0.7f * dPitch + 0.3f * rng.bi();
        dCut = 0.7f * dCut + 0.3f * rng.bi();
        if (retrigger || wasIdle) {
            e1.gateOn(); e2.gateOn(); e3.gateOn();
            const int mode = c.p->lfo1Mode;
            const bool lf = c.p->lfo1Wave < kLHF;
            if (lf && (mode == 1 || mode == 2)) {            // once / reset: start the cycle again
                lfo1L.ph = 0; lfo1L.done = false;
                lfo1R.ph = uint32_t(double(c.p->lfo1Phase) * 4294967295.0); lfo1R.done = false;
            }
            lfo1Time = 0.0f;
            lfo2Time = 0.0f;
        }
    }
    void release(uint64_t when) {
        gate = false;
        relAge = when;
        e1.gateOff(); e2.gateOff(); e3.gateOff();
    }
    bool finished(const LayerParams& p) const {
        if (p.vcaSrc == 2) return false;                    // VCA held open: it sounds until stolen
        return p.vcaSrc == 1 ? e3.idle() : e2.idle();
    }

    // Renders n samples at the internal rate and adds them to L and R.
    void render(BlockCtx& c, float* L, float* R, const float* const* matrixSrcShared);
};

// ---------------------------------------------------------------------------------------
inline void Voice::render(BlockCtx& c, float* outL, float* outR, const float* const* /*unused*/) {
    const LayerParams& p = *c.p;
    const int nEars = c.binaural ? 2 : 1;
    const float invFs = c.invFs;

    // Envelope settings, with drift on their times and the matrix's attack/decay (from this block's start).
    const float dEnvK = (1.0f + 0.15f * c.driftAmt * dEnv) * varyEnvK;
    const float atkK = exp2Fast(3.0f * modOut[kMdEnv1Attack]), decK = exp2Fast(3.0f * modOut[kMdEnv1Decay]);
    e1.set(c.e1T[1] * dEnvK * atkK, c.e1T[2] * dEnvK * decK, c.e1T[3], c.e1T[4] * dEnvK * decK, c.e1T[0], p.e1Mode == 2, c.fs);
    e2.set(c.e2T[0] * dEnvK, c.e2T[1] * dEnvK, c.e2T[2], c.e2T[3] * dEnvK, 0.0f, false, c.fs);
    e3.set(c.e3T[1] * dEnvK, c.e3T[2] * dEnvK, c.e3T[3], c.e3T[4] * dEnvK, c.e3T[0], p.e3Mode == 2, c.fs);

    const float sisterW[4] = {c.tierW[0], c.tierW[1], c.tierW[2], c.tierW[3]};
    const bool cross = p.pitchSrc == 2 && p.pitchMod > 0.0f;
    const float xIdx = 6.0f * p.pitchMod * p.pitchMod;
    const float pmEnvSemis = 48.0f * p.pitchMod * p.pitchMod, pmLfoSemis = 24.0f * p.pitchMod * p.pitchMod;
    const float vcfEnvOct = 9.0f * p.vcfMod, vcfLfoOct = 4.0f * p.vcfMod * p.vcfMod, vcfOscOct = 6.0f * p.vcfMod * p.vcfMod;
    const float ktOct = (p.keytrack == 2 ? 1.0f : (p.keytrack == 1 ? 0.5f : 0.0f)) / 12.0f;
    const float l2PitchSemis = 12.0f * p.lfo2Pitch * p.lfo2Pitch, l2VcfOct = 4.0f * p.lfo2Vcf * p.lfo2Vcf, l2Vca = p.lfo2Vca;
    const float l2Trig = p.lfo2Trig == 2 ? 1.0f : (p.lfo2Trig == 1 ? std::max(c.modWheel, c.aftertouch) : c.modWheel);
    const float l2RateK = exp2Fast(3.0f * p.lfo2RateMod * l2Trig);
    const uint32_t l2Inc = uint32_t(clampf(c.lfo2Hz * l2RateK * invFs, 0.0f, 0.25f) * 4294967296.0);
    const bool lfoHF = p.lfo1Wave >= kLHF;
    const bool perVoiceLfo = !c.lfo1Shared;
    const float hfOscMix = 0.8f;
    const float velVca = p.velVca >= 0 ? 1.0f + p.velVca * (vel - 1.0f) : 1.0f + p.velVca * vel;
    const float velVcf = p.velVcf >= 0 ? 1.0f + p.velVcf * (vel - 1.0f) : 1.0f + p.velVcf * vel;
    const double glideStep = double(c.glideRate) / double(c.fs);
    const float noteK = (float(note) - 60.0f) / 60.0f;
    const float pwmAmt = p.waveMod;
    const float cutBase = c.cutOct + c.driftAmt * 0.25f * dCut + varyCut;
    const float semisDrift = c.driftAmt * (0.06f * dPitch);   // in semitones (6 cents)
    const float voiceGain = 0.5f;

    // Matrix slots that are in use.
    int ms[kMatrixSlots], md[kMatrixSlots], mv[kMatrixSlots], mc[kMatrixSlots], mi[kMatrixSlots], nm = 0;
    float ma[kMatrixSlots];
    for (int i = 0; i < kMatrixSlots; ++i)
        if (p.mSrc[i] != kMsNone && p.mDst[i] != kMdNone && p.mAmt[i] != 0.0f) {
            ms[nm] = p.mSrc[i]; md[nm] = p.mDst[i]; ma[nm] = p.mAmt[i]; mv[nm] = p.mVia[i]; mc[nm] = p.mCrv[i]; mi[nm] = i; ++nm;
        }
    if (nm == 0) for (auto& m : modOut) m = 0.0f;
    const float smoothK = 1.0f - std::exp(-1.0f / (0.045f * c.fs));       // SMOOTH: about 45 ms

    // The sisters' frequency ratios, for this block.
    float ratio[2][7];
    for (int e = 0; e < nEars; ++e)
        for (int k = 0; k < 7; ++k) ratio[e][k] = k == 0 ? 1.0f : exp2Fast(set[e].cents[k] * c.detuneCents * (1.0f / 1200.0f));

    // Pan, with the matrix's (once a block): in the binaural routing, a balance between the ears.
    const float panMod = clampf(modOut[kMdPan], -1.0f, 1.0f);
    const float panAt = clampf(pan + panMod, -1.0f, 1.0f);
    const float panL = c.binaural ? std::sqrt(1.0f - panMod) : std::sqrt(0.5f * (1.0f - panAt));
    const float panR = c.binaural ? std::sqrt(1.0f + panMod) : std::sqrt(0.5f * (1.0f + panAt));
    float loudest = 0.0f;

    for (int i = 0; i < c.n; ++i) {
        // ---- modulators
        const float env1 = e1.tick(invFs);
        const float env2 = e2.tick(invFs);
        const float env3 = e3.tick(invFs);
        const float fol = c.follow ? c.follow[i] : c.envFollow;

        // LFO 1 (fades in over its delay)
        lfo1Time += invFs;
        lfo1Fade = c.lfo1DelayS <= 0.0f ? 1.0f : std::min(1.0f, lfo1Time / c.lfo1DelayS);
        float lfoL, lfoR;
        if (!perVoiceLfo) {
            lfoL = c.sharedL[i];
            lfoR = c.sharedR[i];
        } else if (!lfoHF) {
            const float rateK = exp2Fast(4.0f * modOut[kMdLfo1Rate]);
            const uint32_t inc = uint32_t(clampf(c.lfo1Hz * rateK * invFs, 0.0f, 0.25f) * 4294967296.0);
            const bool once = p.lfo1Mode == 1;
            const float* user = c.lfoUser[p.lfo1Wave == kLUser2 ? 1 : 0];
            lfoL = lfo1L.tick(p.lfo1Wave, inc, invFs, user, once);
            lfoR = c.binaural ? lfo1R.tick(p.lfo1Wave, inc, invFs, user, once) : lfoL;
        } else {
            // High-frequency modes: a sine, free or tracking the keyboard (RATE sets the interval).
            float hz = c.lfo1Hz;
            if (p.lfo1Wave == kLHFTrack) hz = 440.0f * exp2Fast((float(pitch) + uniSemis + 12.0f * float(p.octave) + c.bendSemis - 69.0f) / 12.0f) * c.lfo1Hz;
            hz *= exp2Fast(4.0f * modOut[kMdLfo1Rate]);
            const uint32_t inc = uint32_t(clampf(hz * invFs, 0.0f, 0.45f) * 4294967296.0);
            ear[0].hfPh += inc;
            const uint32_t offs = uint32_t(double(p.lfo1Phase) * 4294967295.0);
            lfoL = LfoCore::sine(ear[0].hfPh);
            lfoR = c.binaural ? LfoCore::sine(ear[0].hfPh + offs) : lfoL;
        }
        const float lfoDepth = lfo1Fade * clampf(1.0f + modOut[kMdLfo1Depth], 0.0f, 2.0f);
        lfoL *= lfoDepth;
        lfoR *= lfoDepth;

        // LFO 2: free running, one per voice; its depth comes from the wheel, aftertouch, or always.
        lfo2Time += invFs;
        lfo2Fade = c.lfo2DelayS <= 0.0f ? 1.0f : std::min(1.0f, lfo2Time / c.lfo2DelayS);
        {
            const uint32_t o = lfo2Ph;
            lfo2Ph += l2Inc;
            if (lfo2Ph < o) lfo2Hold = rng.bi();
            lfo2Noise += (rng.bi() * 1.0f - lfo2Noise) * std::min(1.0f, 60.0f * invFs);
        }
        const float lfo2 = lfo2Shape(p.lfo2Wave, lfo2Ph, lfo2Hold, lfo2Noise * 4.0f) * l2Trig * lfo2Fade;

        // Drift's slow wander (a new aim every second or so).
        wanderClock += invFs * 0.7f;
        if (wanderClock >= 1.0f) { wanderClock -= 1.0f; wanderTarget = rng.bi(); }
        wander += (wanderTarget - wander) * 2.0f * invFs;

        // ---- the matrix: source, shaped, times the amount, times the VIA source
        if (nm) {
            for (auto& m : modOut) m = 0.0f;
            const float envEff1 = (p.e1Mode == 1 ? -env1 : env1);
            auto source = [&](int which) {
                switch (which) {
                    case kMsOsc2: return ear[0].last[1];
                    case kMsLfo1: return lfoL;
                    case kMsLfo2: return lfo2;
                    case kMsEnv1: return envEff1;
                    case kMsEnv2: return env2;
                    case kMsVelocity: return vel;
                    case kMsAftertouch: return c.aftertouch;
                    case kMsExpression: return c.expression;
                    case kMsEnvFollower: return fol;
                    case kMsNote: return noteK;
                    case kMsModWheel: return c.modWheel;
                    case kMsNoteRnd: return noteRnd;
                    default: return 0.0f;
                }
            };
            for (int k = 0; k < nm; ++k) {
                float s = source(ms[k]);
                if (mc[k] == kCurveExp) s *= std::abs(s);
                float o = s * ma[k];
                if (mv[k] != kMsNone) o *= source(mv[k]);
                if (mc[k] == kCurveStep) o = std::round(o * 12.0f) * (1.0f / 12.0f);
                else if (mc[k] == kCurveSmooth) {
                    float& z = slotZ[mi[k]];
                    if (slotFresh) z = o;
                    z += (o - z) * smoothK;
                    o = z;
                }
                modOut[md[k]] += o;
            }
            slotFresh = false;
        }

        // ---- pitch
        if (pitch != target) {
            if (glideStep <= 0.0) pitch = target;
            else if (pitch < target) pitch = std::min(target, pitch + glideStep);
            else pitch = std::max(target, pitch - glideStep);
        }
        const float env1Out = (p.e1Mode == 1 ? -env1 : env1) * clampf(1.0f + modOut[kMdEnv1Depth], 0.0f, 2.0f);
        const float basePitch = float(pitch) + uniSemis + 12.0f * float(p.octave) + c.bendSemis + semisDrift + c.driftAmt * 0.03f * wander
                                + lfo2 * l2PitchSemis + 12.0f * modOut[kMdPitch];
        const float vcaBase = (p.vcaSrc == 0 ? env2 : (p.vcaSrc == 1 ? (p.e3Mode == 1 ? std::max(0.0f, 1.0f - env3) : env3) : (c.extVca ? fol : 1.0f)));
        float vca = vcaBase * velVca * clampf(1.0f + modOut[kMdVcaLevel], 0.0f, 2.0f);
        if (l2Vca > 0.0f) {
            const bool uni = p.lfo2Wave == kL2Saw || p.lfo2Wave == kL2Square || p.lfo2Wave == kL2Ramp;
            vca *= 1.0f - l2Vca * clampf(uni ? lfo2 : 0.5f + 0.5f * lfo2, 0.0f, 1.0f);
        }
        vca = std::max(0.0f, vca);
        loudest = std::max(loudest, vca);

        float earOut[2] = {0, 0};
        for (int e = 0; e < nEars; ++e) {
            Ear& E = ear[e];
            const float lfo = e == 0 ? lfoL : lfoR;
            const EarSet& S = set[e];

            // The MOD section's sources for this ear.
            const float pmSemis = p.pitchSrc == 0 ? pmEnvSemis * env1Out : (p.pitchSrc == 1 ? pmLfoSemis * lfo : 0.0f);
            float wmU = 1.0f;                                    // 0..1
            if (p.waveSrc == 1) wmU = clampf(env1Out, 0.0f, 1.0f);
            else if (p.waveSrc == 2) wmU = (p.lfo1Wave == kLSaw || p.lfo1Wave == kLSquare || p.lfo1Wave == kLRamp) ? lfo : 0.5f + 0.5f * lfo;
            wmU = clampf(wmU, 0.0f, 1.0f) * pwmAmt;

            float out[2];
            float incBase[2];
            for (int o = 0; o < 2; ++o) {
                const OscCfg& oc = c.osc[o];
                float semis = (oc.drone ? 36.0f : basePitch) + oc.tuneSemis + (oc.pitchMod ? pmSemis : 0.0f);
                incBase[o] = 440.0f * exp2Fast((semis - 69.0f) * (1.0f / 12.0f)) * invFs;
            }
            // Cross modulation (linear, through zero): the other oscillator's last output.
            float fm[2] = {0, 0};
            if (cross) {
                if (p.modOsc == 0) fm[0] = xIdx * E.last[1];
                else if (p.modOsc == 2) fm[1] = xIdx * E.last[0];
                else { fm[0] = xIdx * E.last[1]; fm[1] = xIdx * E.last[0]; }
            }
            fm[0] += 2.0f * modOut[kMdOsc2Osc1] * E.last[1];

            // Hard sync: who is master.
            const int slave = p.sync == 1 ? 1 : (p.sync == 2 ? 0 : -1);
            const int master = slave == 1 ? 0 : 1;
            int masterN = 1;
            float syncD[7];
            bool syncHit[7];
            for (int k = 0; k < 7; ++k) syncHit[k] = false;

            // Process the master first so the slave knows when it wrapped.
            const int order[2] = {slave == 0 ? 1 : 0, slave == 0 ? 0 : 1};
            constexpr int kMaxJumps = 64;
            int nJump = 0;
            float jumpH[kMaxJumps], jumpD[kMaxJumps];
            auto addJump = [&](float h, float d) { if (nJump < kMaxJumps) { jumpH[nJump] = h; jumpD[nJump] = clampf(d, 0.0f, 1.0f); ++nJump; } };
            for (int oi = 0; oi < 2; ++oi) {
                const int o = order[oi];
                const OscCfg& oc = c.osc[o];
                const int nSub = oc.detuned ? S.n : 1;
                if (o == master) masterN = nSub;
                float wave = 0.0f;
                if (oc.kind == 2) {                                  // noise, coloured by the wave mod
                    const float white = E.rng.bi();
                    const float colour = oc.waveMod ? wmU : 0.0f;
                    const float a = 1.0f - 0.97f * colour;
                    E.noiseLp[o] += (white - E.noiseLp[o]) * a;
                    wave = E.noiseLp[o] * (1.0f + 1.5f * colour);
                } else {
                    float morph = 0.0f, pw = oc.pwBase;
                    const bool morphs = oc.kind == 0 || oc.kind == 3;
                    if (oc.waveMod) {
                        if (morphs) morph = wmU;
                        else pw = clampf(oc.pwBase - oc.pwSpan * wmU, 0.03f, 0.97f);
                    }
                    if (o == 0) {
                        if (morphs) morph = clampf(morph + modOut[kMdOsc1Wave], 0.0f, 1.0f);
                        else pw = clampf(pw - 0.45f * modOut[kMdOsc1Wave], 0.03f, 0.97f);
                    }
                    const uint32_t pwU = uint32_t(double(pw) * 4294967295.0);
                    // A synced saw, square or pulse runs as its exact (naive) shape, every jump in it
                    // smoothed by the band-limited step at its exact time.
                    const bool naive = o == slave && (oc.kind == 1 || oc.kind == 3);
                    auto nval = [&](double f) -> float {
                        f -= std::floor(f);
                        auto saw = [](double x) { x -= std::floor(x); return float(2.0 * x - 1.0); };
                        return oc.kind == 1 ? saw(f) - saw(f + pw) : saw(f) - morph * saw(f + 0.5);
                    };
                    const double th[2] = {1.0, oc.kind == 1 ? 1.0 - double(pw) : 0.5};
                    const float thH[2] = {-2.0f, oc.kind == 1 ? 2.0f : 2.0f * morph};
                    float sum = 0.0f;
                    for (int k = 0; k < nSub; ++k) {
                        float inc = incBase[o] * ratio[e][k] * (1.0f + fm[o]);
                        inc = clampf(inc, -0.45f, 0.45f);
                        const int32_t incI = int32_t(inc * 4294967296.0f);
                        const uint32_t old = E.ph[o][k];
                        uint32_t ph = old + uint32_t(incI);
                        const int lvl = levelFor(std::abs(inc));
                        auto val = [&](uint32_t q) -> float {
                            if (oc.kind == 1) {
                                const float* t = oc.a->level(lvl);
                                return WaveTable::read(t, q) - WaveTable::read(t, q + pwU);
                            }
                            if (oc.kind == 3) {                     // saw, morphing to a square
                                const float* t = oc.a->level(lvl);
                                const float s0 = WaveTable::read(t, q);
                                return morph <= 0.0f ? s0 : s0 - morph * WaveTable::read(t, q + 0x80000000u);
                            }
                            const float va = WaveTable::read(oc.a->level(lvl), q);
                            if (morph <= 0.0f) return va;
                            return va + (WaveTable::read(oc.b->level(lvl), q) - va) * morph;
                        };
                        if (o == master && slave >= 0 && incI > 0 && ph < old) {
                            syncHit[k] = true;
                            syncD[k] = float(ph) / float(uint32_t(incI));   // samples since the wrap, 0..1
                        }
                        const float w = oc.detuned ? sisterW[S.tier[k]] * c.earGain : 1.0f;
                        if (naive && incI > 0) {
                            const double incC = double(uint32_t(incI)) * (1.0 / 4294967296.0);
                            const double a = double(old) * (1.0 / 4294967296.0);
                            const int mk = k < masterN ? k : 0;
                            const bool hit = syncHit[mk];
                            const double dHit = hit ? double(clampf(syncD[mk], 0.0f, 1.0f)) : 0.0;
                            const double tauR = 1.0 - dHit;                 // when in this sample the reset comes (or its end)
                            auto edges = [&](double from, double span, double t0) {
                                for (int q = 0; q < 2; ++q) {
                                    if (thH[q] == 0.0f) continue;
                                    for (double x = th[q] + std::floor(from - th[q]) + 1.0; x <= from + span; x += 1.0)
                                        addJump(thH[q] * w, float(1.0 - (t0 + (x - from) / incC)));
                                }
                            };
                            edges(a, tauR * incC, 0.0);
                            double np;
                            if (hit) {
                                addJump((nval(0.0) - nval(a + tauR * incC)) * w, float(dHit));
                                edges(0.0, dHit * incC, tauR);
                                np = dHit * incC;
                            } else {
                                np = a + incC;
                            }
                            np -= std::floor(np);
                            E.ph[o][k] = uint32_t(np * 4294967296.0);
                            sum += w * nval(np);
                            continue;
                        }
                        if (o == slave) {
                            const int mk = k < masterN ? k : 0;
                            if (syncHit[mk] && incI > 0) {
                                const float d = clampf(syncD[mk], 0.0f, 1.0f);
                                const uint32_t atReset = old + uint32_t(double(incI) * (1.0 - double(d)));
                                const float before = val(atReset);
                                const float after = val(0u);
                                addJump((after - before) * w, d);
                                ph = uint32_t(double(incI) * double(d));
                            }
                        }
                        E.ph[o][k] = ph;
                        sum += w * val(ph);
                    }
                    wave = sum;
                }
                out[o] = wave;
            }
            // The 8-sample delay that lets sync's steps be band-limited.
            {
                const BlepTable& bt = BlepTable::get();
                const int pos = E.rpos;
                for (int o = 0; o < 2; ++o) {
                    E.last[o] = out[o];
                    E.ring[o][pos] += out[o];
                    if (o == slave)
                        for (int jn = 0; jn < nJump; ++jn)
                            for (int j = 0; j < BlepTable::kTaps; ++j) E.ring[o][(pos + j - BlepTable::kHalf) & (BlepTable::kTaps - 1)] += jumpH[jn] * bt.at(jumpD[jn], j);
                    const int oi = (pos - BlepTable::kHalf) & (BlepTable::kTaps - 1);
                    out[o] = E.ring[o][oi];
                    E.ring[o][oi] = 0.0f;
                }
                E.rpos = (pos + 1) & (BlepTable::kTaps - 1);
            }
            // LFO 1 at audio rate can join an oscillator's channel.
            if (lfoHF && p.lfo1Mode > 0) out[p.lfo1Mode - 1] += hfOscMix * lfo;

            // ---- mixer, driven into the filter
            const float g1 = c.g1 * clampf(1.0f + modOut[kMdOsc1Mix], 0.0f, 2.0f);
            const float g2 = c.g2 * clampf(1.0f + modOut[kMdOsc2Mix], 0.0f, 2.0f);
            float mixIn = out[0] * g1 + out[1] * g2;
            if (c.extL) mixIn += c.gExt * (e == 1 && c.extR ? c.extR[i] : c.extL[i]);
            if (std::abs(mixIn) > 1.0f) c.odrive = true;
            const float x = kneeSat(mixIn, 0.8f, 1.4f) + 1e-5f * E.rng.bi();     // a breath of noise, as in any analog circuit

            // ---- filter
            float cutOct = cutBase + ktOct * (float(pitch) + uniSemis - 60.0f) + c.bendVcfOct + lfo2 * l2VcfOct + 6.0f * modOut[kMdCutoff];
            if (p.vcfSrc == 0) cutOct += (vcfEnvOct * velVcf + 9.0f * modOut[kMdEnv1Cutoff]) * env1Out;
            else {
                cutOct += 9.0f * modOut[kMdEnv1Cutoff] * env1Out;
                cutOct += p.vcfSrc == 1 ? vcfLfoOct * lfo : vcfOscOct * E.last[1];
            }
            const float delta = clampf(p.deltaCutoff + modOut[kMdDeltaCutoff], -1.0f, 1.0f) * 1.5f;   // +-1.5 octaves each way
            const float kRes = clampf(c.kRes + 4.3f * modOut[kMdResonance], 0.0f, 4.6f);
            const bool t1 = c.lfComp > 0.0f;
            const float kUse = t1 ? kRes * kType1Gain : kRes;
            float ghA = 0.0f, ghB = 0.0f;
            auto gOf = [&](float oct, float& gh) {
                float hz = 16.0f * exp2Fast(clampf(oct, -1.0f, 11.5f));
                if (t1) {
                    hz *= kType1Tune;
                    gh = tanPade(float(kPi) * std::min(0.25f * hz * invFs, 0.42f));
                }
                return tanPade(float(kPi) * std::min(hz * invFs, 0.42f));
            };
            float y;
            if (c.route == kRouteStereo) {
                const float g = gOf(cutOct + (e == 0 ? delta : -delta), ghA);
                y = E.fa.process(x, g, kUse, ghA, c.mixA);
            } else if (c.route == kRouteParallel) {
                const float ga = gOf(cutOct + delta, ghA), gb = gOf(cutOct - delta, ghB);
                y = 0.5f * (E.fa.process(x, ga, kUse, ghA, c.mixA) + E.fb.process(x, gb, kUse, ghB, c.mixB));
            } else {
                const float ga = gOf(cutOct + delta, ghA), gb = gOf(cutOct - delta, ghB);
                const float ya = E.fa.process(x, ga, kUse, ghA, c.mixA);
                y = E.fb.process(ya, gb, kUse, ghB, c.mixB);
            }
            earOut[e] = y * vca * voiceGain;
        }
        if (c.binaural) {
            outL[i] += earOut[0] * panL;
            outR[i] += earOut[1] * panR;
        } else {
            outL[i] += earOut[0] * panL;
            outR[i] += earOut[0] * panR;
        }
    }
    meter = loudest;
}

}  // namespace td
