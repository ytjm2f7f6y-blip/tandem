// Tandem: small DSP building blocks shared by the engine. Header-only, C++17, no dependencies.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

namespace td {

constexpr double kPi = 3.14159265358979323846;
constexpr float kTwoPiF = 6.28318530718f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// 2^x, accurate to about 2e-6 relative (0.003 cents) for |x| < 60.
inline float exp2Fast(float x) {
    x = clampf(x, -60.0f, 60.0f);
    const float fl = std::floor(x);
    const float f = x - fl;
    const float p = 1.0f + f * (0.693147180f + f * (0.240226507f + f * (0.0555041087f + f * (0.00961812911f + f * (0.00133335581f + f * 0.000154035304f)))));
    int32_t bits;
    std::memcpy(&bits, &p, 4);
    bits += int32_t(fl) << 23;
    float r;
    std::memcpy(&r, &bits, 4);
    return r;
}

// tan(x) for 0 <= x < 1.5 (Pade [5/4]; 0.005% at x = 1.26, which is 0.4 * fs).
inline float tanPade(float x) {
    const float x2 = x * x;
    return x * (945.0f - 105.0f * x2 + x2 * x2) / (945.0f - 420.0f * x2 + 15.0f * x2 * x2);
}

// A smooth saturator: tanh to within 2% below |x| = 3, exactly +-1 beyond.
inline float tanhFast(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Fast, decent random numbers (xorshift32).
struct Rng {
    uint32_t s = 0x9E3779B9u;
    Rng() = default;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return float(next() >> 8) * (1.0f / 16777216.0f); }       // [0, 1)
    float bi() { return uni() * 2.0f - 1.0f; }                                // [-1, 1)
};

// One-pole low pass for control signals.
struct Smooth {
    float y = 0.0f, a = 1.0f;
    void setTime(float seconds, float fs) { a = seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * fs)); }
    void reset(float v) { y = v; }
    float tick(float x) { y += (x - y) * a; return y; }
};

// One-pole TPT filter (low and high pass outputs) for tone controls and damping.
struct OnePole {
    float s = 0.0f, g = 0.0f;
    void set(float hz, float fs) { g = std::tan(float(kPi) * clampf(hz, 1.0f, 0.49f * fs) / fs); g = g / (1.0f + g); }
    float lp(float x) { const float v = (x - s) * g; const float y = v + s; s = y + v; return y; }
    float hp(float x) { return x - lp(x); }
    void reset() { s = 0.0f; }
};

// RBJ biquad (transposed direct form II), used by the equaliser.
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float tick(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void set(int type, float f0, float q, float dbGain, float fs) {   // 0 peak, 1 low shelf, 2 high shelf
        const float A = std::pow(10.0f, dbGain / 40.0f);
        const float w = 2.0f * float(kPi) * clampf(f0, 10.0f, 0.45f * fs) / fs;
        const float cw = std::cos(w), sw = std::sin(w);
        float B0, B1, B2, A0, A1, A2;
        if (type == 0) {
            const float al = sw / (2.0f * q);
            B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A;
        } else {
            const float al = sw / 2.0f * std::sqrt(2.0f);        // shelf slope 1
            const float sA = 2.0f * std::sqrt(A) * al;
            if (type == 1) {
                B0 = A * ((A + 1) - (A - 1) * cw + sA); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sA);
                A0 = (A + 1) + (A - 1) * cw + sA; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sA;
            } else {
                B0 = A * ((A + 1) + (A - 1) * cw + sA); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sA);
                A0 = (A + 1) - (A - 1) * cw + sA; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sA;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};

// Soft knee: straight through below t, then a smooth bend towards +-L.
inline float kneeSat(float x, float t, float L) {
    const float a = std::abs(x);
    if (a <= t) return x;
    const float y = t + (L - t) * tanhFast((a - t) / (L - t));
    return x < 0.0f ? -y : y;
}

// A band-limited step's residual (the band-limited step minus the plain one), for smoothing
// the jumps hard sync makes. 16 taps, a Kaiser-windowed sinc at 0.45 of the running rate;
// tab(d, j) is the residual for sample n - 8 + j when the jump happened d samples before n.
struct BlepTable {
    static constexpr int kTaps = 16, kHalf = 8, kRes = 256;
    float t[kRes + 1][kTaps];
    static const BlepTable& get() { static BlepTable b; return b; }
    float at(float d, int j) const {
        const float x = d * kRes;
        const int i = std::min(kRes - 1, int(x));
        const float f = x - float(i);
        return t[i][j] + (t[i + 1][j] - t[i][j]) * f;
    }
private:
    BlepTable() {
        const double fc = 0.45, beta = 9.0;
        auto bessel0 = [](double x) { double s = 1, term = 1; for (int k = 1; k < 30; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); s += term; } return s; };
        auto h = [&](double tt) {
            if (std::abs(tt) >= kHalf) return 0.0;
            const double a = 2 * fc * tt;
            const double sinc = std::abs(a) < 1e-12 ? 1.0 : std::sin(kPi * a) / (kPi * a);
            const double r = tt / kHalf;
            return 2 * fc * sinc * bessel0(beta * std::sqrt(1 - r * r)) / bessel0(beta);
        };
        // The step: cumulative integral of h on a fine grid over [-8, 8].
        const int sub = 512, N = 2 * kHalf * sub;
        std::vector<double> cum(size_t(N) + 1, 0.0);
        for (int i = 0; i < N; ++i) {
            const double t0 = -kHalf + double(i) / sub, t1 = t0 + 1.0 / sub;
            cum[size_t(i) + 1] = cum[size_t(i)] + 0.5 * (h(t0) + h(t1)) / sub;
        }
        const double total = cum[size_t(N)];
        auto step = [&](double x) {
            if (x <= -kHalf) return 0.0;
            if (x >= kHalf) return 1.0;
            const double p = (x + kHalf) * sub;
            const int i = std::min(N - 1, int(p));
            const double f = p - i;
            return (cum[size_t(i)] + (cum[size_t(i) + 1] - cum[size_t(i)]) * f) / total;
        };
        for (int di = 0; di <= kRes; ++di) {
            const double d = double(di) / kRes;
            for (int j = 0; j < kTaps; ++j) {
                const double x = double(j - kHalf) + d;
                t[di][j] = float(step(x) - (j >= kHalf ? 1.0 : 0.0));   // by tap, so d = 1 joins up with d = 0 of the next
            }
        }
    }
};

// In-place radix-2 FFT (sign -1 forward, +1 inverse, unscaled).
inline void fft(std::vector<std::complex<double>>& a, int sign) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = sign * 2.0 * kPi / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Halfband low pass for coming down from twice the rate: 95 taps, flat to 0.2154 fs2
// (19 kHz at 44.1 kHz), 108 dB down from 0.2846 fs2 (25.1 kHz). Taps beside the centre
// (the others are zero, the centre is 0.5).
constexpr int kHbTaps = 24;
constexpr float kHb[kHbTaps] = {
    3.176487888058e-01f, -1.041348731289e-01f, 6.043019672626e-02f, -4.104876360761e-02f, 2.984641540455e-02f, -2.243347042756e-02f,
    1.712896408484e-02f, -1.315114690502e-02f, 1.008535884052e-02f, -7.688145435180e-03f, 5.803762780217e-03f, -4.324623304018e-03f,
    3.171306263436e-03f, -2.281836877831e-03f, 1.605886456502e-03f, -1.101486977665e-03f, 7.332456436093e-04f, -4.712281428367e-04f,
    2.903705185135e-04f, -1.699262295900e-04f, 9.313194166635e-05f, -4.672559216496e-05f, 2.058538990931e-05f, -7.726549952337e-06f};

// Two samples in, one out. Latency: 47 samples at the high rate (23.5 at the low).
// With pairs (x0, x1) = (x[2m], x[2m+1]) and y[m] = sum_j h[j] x[2m+1-j], the side taps
// h[c +- (2k+1)] = kHb[k] all land on the odd samples x1, and the centre tap on x0 from
// 23 pairs ago.
struct Decimator2 {
    static constexpr int N = 2 * kHbTaps;               // 48 odd samples under the side taps
    float hist[2 * N] = {};                             // a doubled ring, so the window is contiguous
    float centre[kHbTaps] = {};                         // x0, delayed 23 pairs
    int pos = 0, cpos = 0;
    void reset() { std::memset(hist, 0, sizeof hist); std::memset(centre, 0, sizeof centre); pos = cpos = 0; }
    float process(float x0, float x1) {
        hist[pos] = x1;
        hist[pos + N] = x1;
        const float* w = hist + pos + 1;                // w[0] oldest .. w[N-1] newest
        float acc = 0.0f;
        for (int k = 0; k < kHbTaps; ++k) acc += kHb[k] * (w[kHbTaps - 1 - k] + w[kHbTaps + k]);
        centre[cpos] = x0;
        cpos = (cpos + 1) % kHbTaps;                    // now the oldest: written 23 pairs ago
        acc += 0.5f * centre[cpos];
        pos = (pos + 1) % N;
        return acc;
    }
};

}  // namespace td
