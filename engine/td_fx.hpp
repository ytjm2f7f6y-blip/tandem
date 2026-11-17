// Tandem: each layer's effects. Inserts in series (drive, chorus, equaliser) and two send
// effects fed by the FX SEND control (a tempo-synced delay and a reverb).
#pragma once

#include "td_dsp.hpp"
#include "td_params.hpp"
#include "td_wave.hpp"

namespace td {

// Hermite interpolation into a ring buffer (size a power of two), `pos` samples back from `w`.
inline float ringRead(const std::vector<float>& buf, int mask, int w, float pos) {
    const float fp = std::floor(pos);
    const float f = pos - fp;
    const int i = w - int(fp);
    const float xm1 = buf[size_t((i + 1) & mask)], x0 = buf[size_t(i & mask)], x1 = buf[size_t((i - 1) & mask)], x2 = buf[size_t((i - 2) & mask)];
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

inline int pow2At(int n) { int p = 1; while (p < n) p <<= 1; return p; }

// ---------------------------------------------------------------------------------------
// Drive: runs at the layer's internal rate, so its harmonics fold over far less.
struct Drive {
    OnePole tone[2], dc[2];
    float fs = 88200.0f;
    void prepare(float rate) { fs = rate; for (auto& t : tone) t.reset(); for (auto& d : dc) { d.reset(); d.set(12.0f, fs); } }
    void process(const LayerParams& p, float* L, float* R, int n) {
        if (p.drvMode == kDriveOff) return;
        const float pre = std::pow(10.0f, (p.drvAmt * 36.0f) / 20.0f);
        const float post = 1.0f / (1.0f + 1.6f * p.drvAmt);
        const float toneHz = 800.0f * std::pow(25.0f, p.drvTone);
        for (auto& t : tone) t.set(toneHz, fs);
        const float wet = p.drvMix, dry = 1.0f - p.drvMix;
        float* ch[2] = {L, R};
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < n; ++i) {
                const float x = ch[c][i];
                const float u = x * pre;
                float y;
                if (p.drvMode == kDriveHarmonic) y = tanhFast(u + 0.35f) - 0.33637f;            // biased: even harmonics, like a valve
                else if (p.drvMode == kDriveOverdrive) y = tanhFast(u);
                else { y = u > 0.0f ? 0.8f * (1.0f - std::exp(-2.2f * u)) : -(1.0f - std::exp(1.4f * u)); }   // asymmetric fuzz
                y = dc[c].hp(y);
                y = tone[c].lp(y) * post;
                ch[c][i] = dry * x + wet * y;
            }
        }
    }
};

// ---------------------------------------------------------------------------------------
// Chorus: one modulated delay per side, their sines a quarter turn apart.
struct Chorus {
    std::vector<float> buf[2];
    int mask = 0, w = 0;
    float fs = 44100.0f;
    uint32_t ph = 0;
    Smooth delayS;
    void prepare(float rate) {
        fs = rate;
        const int n = pow2At(int(rate * 0.06f) + 8);
        for (auto& b : buf) b.assign(size_t(n), 0.0f);
        mask = n - 1;
        w = 0;
        delayS.setTime(0.08f, fs);
        delayS.reset(-1.0f);
    }
    void process(const LayerParams& p, float* L, float* R, int n) {
        if (!p.chOn) return;
        const float baseMs = 1.5f + 18.0f * p.chDelay;
        if (delayS.y < 0.0f) delayS.reset(baseMs);
        const float depthMs = std::min(baseMs * 0.9f, 6.0f) * p.chDepth;
        const float hz = 0.05f * std::pow(100.0f, p.chRate);
        const uint32_t inc = uint32_t(hz / fs * 4294967296.0);
        const float mix = p.chMix;
        const float* sine = WaveBank::get().classic(WaveBank::kSine).level(0);
        for (int i = 0; i < n; ++i) {
            const float base = delayS.tick(baseMs);
            buf[0][size_t(w)] = L[i];
            buf[1][size_t(w)] = R[i];
            ph += inc;
            const float mL = WaveTable::read(sine, ph), mR = WaveTable::read(sine, ph + 0x40000000u);
            const float dL = (base + depthMs * mL) * 0.001f * fs, dR = (base + depthMs * mR) * 0.001f * fs;
            const float wl = ringRead(buf[0], mask, w, std::max(1.0f, dL));
            const float wr = ringRead(buf[1], mask, w, std::max(1.0f, dR));
            L[i] = L[i] * (1.0f - 0.5f * mix) + wl * mix;
            R[i] = R[i] * (1.0f - 0.5f * mix) + wr * mix;
            w = (w + 1) & mask;
        }
    }
};

// ---------------------------------------------------------------------------------------
// The 3-band equaliser: low shelf (150 Hz), a bell with adjustable width (1.2 kHz), high shelf (5 kHz).
struct Eq3 {
    Biquad b[2][3];
    float fs = 44100.0f, lastL = 1e9f, lastM = 1e9f, lastH = 1e9f, lastQ = 0.0f;
    void prepare(float rate) { fs = rate; lastL = lastM = lastH = 1e9f; for (auto& c : b) for (auto& q : c) q.reset(); }
    void process(const LayerParams& p, float* L, float* R, int n) {
        if (p.eqLow != lastL || p.eqMid != lastM || p.eqHigh != lastH || p.eqQ != lastQ) {
            for (auto& c : b) {
                c[0].set(1, 150.0f, 0.7f, p.eqLow, fs);
                c[1].set(0, 1200.0f, std::max(0.2f, p.eqQ), p.eqMid, fs);
                c[2].set(2, 5000.0f, 0.7f, p.eqHigh, fs);
            }
            lastL = p.eqLow; lastM = p.eqMid; lastH = p.eqHigh; lastQ = p.eqQ;
        }
        if (p.eqLow == 0.0f && p.eqMid == 0.0f && p.eqHigh == 0.0f) return;
        float* ch[2] = {L, R};
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i) ch[c][i] = b[c][2].tick(b[c][1].tick(b[c][0].tick(ch[c][i])));
    }
};

// ---------------------------------------------------------------------------------------
// Delay: always in step with the tempo. A change of time slides (like tape). Freeze stops
// new sound getting in and lets what is there go round for ever.
struct Delay {
    std::vector<float> buf[2];
    int mask = 0, w = 0;
    float fs = 44100.0f;
    Smooth timeS;
    OnePole damp[2];
    float fbGain = 0.0f, inGain = 1.0f;
    void prepare(float rate) {
        fs = rate;
        const int n = pow2At(int(rate * 6.5f));
        for (auto& b : buf) b.assign(size_t(n), 0.0f);
        mask = n - 1;
        w = 0;
        timeS.setTime(0.12f, fs);
        timeS.reset(-1.0f);
        for (auto& d : damp) d.reset();
    }
    void process(const LayerParams& p, float bpm, float timeMod, const float* inL, const float* inR, float* outL, float* outR, int n) {
        const float beats = kDelayBeats[std::clamp(p.dlyDiv, 0, kNumDelayDivs - 1)];
        float target = beats * 60.0f / std::max(20.0f, bpm) * exp2Fast(timeMod);
        target = clampf(target, 0.001f, 6.0f) * fs;
        if (timeS.y < 0.0f) timeS.reset(target);
        const float dampHz = p.freeze ? 22000.0f : 20000.0f * std::pow(0.075f, p.dlyDamp);
        for (auto& d : damp) d.set(dampHz, fs);
        const float fbT = p.freeze ? 1.0f : p.dlyFb, inT = p.freeze ? 0.0f : 1.0f;
        const float a = 1.0f - std::exp(-1.0f / (0.01f * fs));
        for (int i = 0; i < n; ++i) {
            fbGain += (fbT - fbGain) * a;
            inGain += (inT - inGain) * a;
            const float d = std::max(2.0f, timeS.tick(target));
            const float yl = ringRead(buf[0], mask, w - 1, d - 1.0f);
            const float yr = ringRead(buf[1], mask, w - 1, d - 1.0f);
            float xl = inL[i] * inGain, xr = inR[i] * inGain;
            float fl = damp[0].lp(yl) * fbGain, fr = damp[1].lp(yr) * fbGain;
            if (p.dlyMode == kDelayPingPong) {
                const float m = 0.5f * (xl + xr);
                buf[0][size_t(w)] = m + fr;
                buf[1][size_t(w)] = fl;
            } else if (p.dlyMode == kDelayMono) {
                const float m = 0.5f * (xl + xr) + 0.5f * (fl + fr);
                buf[0][size_t(w)] = m;
                buf[1][size_t(w)] = m;
            } else {
                buf[0][size_t(w)] = xl + fl;
                buf[1][size_t(w)] = xr + fr;
            }
            // Keep a runaway in bounds without touching ordinary levels.
            for (auto& b : buf) { float& v = b[size_t(w)]; if (std::abs(v) > 2.0f) v = 2.0f * tanhFast(v * 0.5f); }
            outL[i] += yl;
            outR[i] += yr;
            w = (w + 1) & mask;
        }
    }
};

// ---------------------------------------------------------------------------------------
// Reverb: an eight-line feedback delay network (Hadamard mixing), with input diffusion,
// pre-delay, low cut, damping in the loop and a gentle wobble on two lines.
struct Reverb {
    static constexpr int kLines = 8;
    std::vector<float> line[kLines];
    int lmask[kLines] = {}, lw[kLines] = {};
    float len[kLines] = {};
    OnePole damp[kLines];
    struct Ap { std::vector<float> b; int n = 1, i = 0; float g = 0.6f;
        float tick(float x) { const float d = b[size_t(i)]; const float v = x + g * d; b[size_t(i)] = v; i = (i + 1) % n; return d - g * v; } };
    Ap ap[2][4];
    std::vector<float> pre[2];
    int pmask = 0, pw = 0;
    OnePole lowCut[2];
    float fs = 44100.0f;
    uint32_t wob = 0;
    float lastSize = -1.0f;
    float gain[kLines] = {};

    void prepare(float rate) {
        fs = rate;
        static const float ms[kLines] = {29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 59.3f, 67.1f, 73.3f};
        for (int k = 0; k < kLines; ++k) {
            const int n = pow2At(int(ms[k] * 0.001f * 1.7f * fs) + 64);
            line[k].assign(size_t(n), 0.0f);
            lmask[k] = n - 1;
            lw[k] = 0;
            damp[k].reset();
        }
        static const float apMs[4] = {4.7f, 3.6f, 12.7f, 9.3f};
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k) {
                ap[c][k].n = std::max(1, int(apMs[k] * (c ? 1.07f : 1.0f) * 0.001f * fs));
                ap[c][k].b.assign(size_t(ap[c][k].n), 0.0f);
                ap[c][k].i = 0;
                ap[c][k].g = k < 2 ? 0.7f : 0.6f;
            }
        const int pn = pow2At(int(0.21f * fs) + 4);
        for (auto& b : pre) b.assign(size_t(pn), 0.0f);
        pmask = pn - 1;
        pw = 0;
        lastSize = -1.0f;
    }

    void process(const LayerParams& p, const float* inL, const float* inR, float* outL, float* outR, int n, float level) {
        static const float ms[kLines] = {29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 59.3f, 67.1f, 73.3f};
        const float scale = 0.4f + 1.2f * p.rvSize;
        const float rt60 = 0.4f * std::pow(50.0f, p.rvSize);
        for (int k = 0; k < kLines; ++k) {
            len[k] = ms[k] * 0.001f * scale * fs;
            gain[k] = std::pow(10.0f, -3.0f * len[k] / (rt60 * fs));
            damp[k].set(18000.0f * std::pow(0.11f, p.rvDamp), fs);
        }
        for (auto& lc : lowCut) lc.set(20.0f * std::pow(40.0f, p.rvLowCut), fs);
        const float preD = std::max(1.0f, 0.2f * p.rvPreDly * p.rvPreDly * fs + 1.0f);
        const uint32_t wInc = uint32_t(0.6f / fs * 4294967296.0);
        const float* sine = WaveBank::get().classic(WaveBank::kSine).level(0);
        const float width = p.rvWidth;
        for (int i = 0; i < n; ++i) {
            pre[0][size_t(pw)] = inL[i];
            pre[1][size_t(pw)] = inR[i];
            float xl = ringRead(pre[0], pmask, pw, preD), xr = ringRead(pre[1], pmask, pw, preD);
            pw = (pw + 1) & pmask;
            xl = lowCut[0].hp(xl);
            xr = lowCut[1].hp(xr);
            for (int k = 0; k < 4; ++k) { xl = ap[0][k].tick(xl); xr = ap[1][k].tick(xr); }
            wob += wInc;
            const float wv = WaveTable::read(sine, wob) * 0.0004f * fs;
            float y[kLines];
            for (int k = 0; k < kLines; ++k) {
                const float d = len[k] + (k == 2 ? wv : (k == 5 ? -wv : 0.0f));
                y[k] = damp[k].lp(ringRead(line[k], lmask[k], lw[k] - 1, d - 1.0f)) * gain[k];
            }
            // Hadamard 8 (fast Walsh-Hadamard), scaled to keep energy.
            for (int h = 1; h < kLines; h <<= 1)
                for (int a = 0; a < kLines; a += 2 * h)
                    for (int b = a; b < a + h; ++b) { const float u = y[b], v = y[b + h]; y[b] = u + v; y[b + h] = u - v; }
            const float hs = 0.35355339f;
            float ol = 0.0f, orr = 0.0f;
            for (int k = 0; k < kLines; ++k) {
                const float v = y[k] * hs;
                const float in = (k & 1) ? xr : xl;
                line[k][size_t(lw[k])] = v + in * 0.5f;
                lw[k] = (lw[k] + 1) & lmask[k];
                if (k & 1) orr += (k & 2 ? -v : v); else ol += (k & 4 ? -v : v);
            }
            const float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * width;
            outL[i] += (mid + side) * level;
            outR[i] += (mid - side) * level;
        }
    }
};

}  // namespace td
