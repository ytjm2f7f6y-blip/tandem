// Tandem: band-limited single-cycle wave tables.
//
// Every wave is kept as 11 tables of 4096 points, one per octave of playing range: table l
// holds harmonics up to 1024 >> l. An oscillator reads the richest table whose top harmonic
// stays under 0.7 x the rate it runs at; at twice 44.1 kHz anything that folds over lands
// above 26 kHz and the decimator removes it, while everything up to 20 kHz is always there.
//
// The 32 alternative waves are Tandem's own (not the hardware's): additive organs, vowels,
// synced and FM shapes, folded and clipped sines and so on, built here from formulas.
// A user wave can be added from 2048 16-bit points (the hardware's .wd4 layout), see addRaw.
#pragma once

#include "td_dsp.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

namespace td {

struct WaveTable {
    static constexpr int kBits = 12, kSize = 1 << kBits, kLevels = 11, kMaxHarm = 1024;
    std::vector<float> data = std::vector<float>(size_t(kLevels) * (kSize + 1), 0.0f);
    std::string name;
    const float* level(int l) const { return data.data() + size_t(l) * (kSize + 1); }

    // From complex harmonic amplitudes c[n] (n = 1..), value(phi) = sum Im(c[n] e^{i 2 pi n phi}).
    // Normalised so the full table peaks at `peak` (or not, if peak <= 0).
    void build(const std::vector<std::complex<double>>& c, float peak) {
        std::vector<std::complex<double>> buf(kSize);
        float norm = 1.0f;
        for (int l = 0; l < kLevels; ++l) {
            const int H = std::min<int>(kMaxHarm >> l, int(c.size()) - 1);
            std::fill(buf.begin(), buf.end(), std::complex<double>(0, 0));
            for (int n = 1; n <= H; ++n) {
                // Im(c e^{i w}) = Re(-i c e^{i w}): put -i c at +n and its conjugate at -n.
                const auto v = std::complex<double>(0, -1) * c[size_t(n)];
                buf[size_t(n)] += 0.5 * v;
                buf[size_t(kSize - n)] += 0.5 * std::conj(v);
            }
            fft(buf, +1);
            float* t = data.data() + size_t(l) * (kSize + 1);
            for (int i = 0; i < kSize; ++i) t[i] = float(buf[size_t(i)].real());
            t[kSize] = t[0];
            if (l == 0 && peak > 0.0f) {
                float m = 1e-9f;
                for (int i = 0; i < kSize; ++i) m = std::max(m, std::abs(t[i]));
                norm = peak / m;
            }
            for (int i = 0; i <= kSize; ++i) t[i] *= norm;
        }
    }

    // From one cycle of samples (any length that is a power of two, >= 64): its harmonics, then build.
    void fromCycle(const std::vector<double>& cyc, float peak, int maxHarm = kMaxHarm) {
        std::vector<std::complex<double>> a(cyc.size());
        for (size_t i = 0; i < cyc.size(); ++i) a[i] = cyc[i];
        fft(a, -1);
        const int H = std::min<int>(maxHarm, int(cyc.size() / 2) - 1);
        std::vector<std::complex<double>> c(size_t(H) + 1);
        // x[i] = sum (A_n e^{i w} + conj) / N; as Im(c e^{i w}): c = 2 i A_n / N.
        for (int n = 1; n <= H; ++n) c[size_t(n)] = std::complex<double>(0, 2.0) * a[size_t(n)] / double(cyc.size());
        build(c, peak);
    }

    // Linear interpolation into table `l` at a 32-bit phase.
    static float read(const float* t, uint32_t ph) {
        const uint32_t i = ph >> (32 - kBits);
        const float f = float(ph & ((1u << (32 - kBits)) - 1u)) * (1.0f / float(1u << (32 - kBits)));
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};

// The level whose top harmonic is under 0.7 x the running rate, for phase increment `inc` (cycles per sample).
inline int levelFor(float incAbs) {
    // Want (1024 >> l) * inc < 0.7, ie l >= log2(1024 * inc / 0.7).
    const float r = incAbs * (1024.0f / 0.7f);
    if (!(r > 1.0f)) return 0;
    // The exponent and mantissa straight from the bits (r is normal and above 1 here): r = m 2^e, m in [0.5, 1).
    uint32_t bits;
    std::memcpy(&bits, &r, 4);
    const int e = int((bits >> 23) & 0xffu) - 126;
    const int l = (bits & 0x7fffffu) == 0 ? e - 1 : e;      // m == 0.5 exactly: a power of two
    return l > WaveTable::kLevels - 1 ? WaveTable::kLevels - 1 : l;
}

class WaveBank {
public:
    enum Classic { kSine = 0, kSaw = 1, kTri = 2, kNumClassic = 3 };
    static constexpr int kNumAlt = 32;

    static const WaveBank& get() {
        static WaveBank bank;
        return bank;
    }
    const WaveTable& classic(int i) const { return classic_[size_t(i)]; }
    const WaveTable& alt(int i) const { return alt_[size_t(clampAlt(i))]; }
    static int clampAlt(int i) { return i < 0 ? 0 : (i >= kNumAlt ? kNumAlt - 1 : i); }
    static const char* altName(int i);

private:
    WaveBank() {
        using C = std::complex<double>;
        {   // sine
            std::vector<C> c(2, C(0, 0));
            c[1] = 1.0;
            classic_[kSine].build(c, 0.0f);
            classic_[kSine].name = "Sine";
        }
        {   // saw rising -1..1: 2 phi - 1 = -(2/pi) sum sin(2 pi n phi) / n
            std::vector<C> c(WaveTable::kMaxHarm + 1, C(0, 0));
            for (int n = 1; n <= WaveTable::kMaxHarm; ++n) c[size_t(n)] = -2.0 / (kPi * n);
            classic_[kSaw].build(c, 0.0f);
            classic_[kSaw].name = "Sawtooth";
        }
        {   // triangle
            std::vector<C> c(WaveTable::kMaxHarm + 1, C(0, 0));
            for (int n = 1; n <= WaveTable::kMaxHarm; n += 2) c[size_t(n)] = (((n - 1) / 2) % 2 ? -1.0 : 1.0) * 8.0 / (kPi * kPi * n * n);
            classic_[kTri].build(c, 0.0f);
            classic_[kTri].name = "Triangle";
        }
        for (int i = 0; i < kNumAlt; ++i) makeAlt(i);
    }

    void makeAlt(int i);

    std::array<WaveTable, kNumClassic> classic_;
    std::array<WaveTable, kNumAlt> alt_;
};

// Your own waves: kUserWaves slots after the built-in alternatives (alternative index kNumAlt + k).
// The plugin builds each table on the message thread and publishes it here; the audio thread
// only reads the pointers (an empty slot plays the first built-in alternative).
constexpr int kUserWaves = 8;
struct UserWaves {
    std::array<std::atomic<const WaveTable*>, kUserWaves> t{};
};

inline const char* WaveBank::altName(int i) {
    static const char* const names[kNumAlt] = {
        "Parlour Organ", "Cathedral",  "Reed",         "Vowel Ah",    "Vowel Eh",     "Vowel Oo",     "Glass",       "Bellish",
        "Hollow",        "Nasal",      "Sync Fifth",   "Sync Tenth",  "FM Octave",    "FM Twelfth",   "FM Growl",    "Folded",
        "Folded Twice",  "Clipped",    "Half Wave",    "Twin Saw",    "Staircase",    "Spike",        "Sub Bass",    "Bowed",
        "Choir",         "Metal",      "Ghost",        "Saw Square",  "Formant Rise", "Chime",        "Pipe",        "Grit"};
    return names[clampAlt(i)];
}

inline void WaveBank::makeAlt(int i) {
    using C = std::complex<double>;
    const int H = WaveTable::kMaxHarm;
    const int N = 4096;
    WaveTable& w = alt_[size_t(i)];
    w.name = altName(i);
    std::vector<C> c(size_t(H) + 1, C(0, 0));
    auto harm = [&](int n, double a, double phase = 0.0) { if (n >= 1 && n <= H) c[size_t(n)] += std::polar(a, phase); };
    auto cycle = [&](auto f) {         // a time-domain definition, phi in [0, 1)
        std::vector<double> x(static_cast<size_t>(N));
        for (int k = 0; k < N; ++k) x[size_t(k)] = f(double(k) / N);
        w.fromCycle(x, 1.0f, 512);     // the wd4 rule: nothing above harmonic 512
    };
    // A vowel: harmonic amplitudes shaped by resonances at fixed harmonic numbers (for a voice near 140 Hz).
    auto vowel = [&](std::initializer_list<std::pair<double, double>> formants) {
        for (int n = 1; n <= 96; ++n) {
            double a = 0.0;
            for (auto [fh, bw] : formants) a += 1.0 / (1.0 + std::pow((n - fh) / bw, 2.0));
            harm(n, a / std::sqrt(double(n)), 0.0);
        }
    };
    Rng rng(0x51EDu + uint32_t(i) * 7919u);
    switch (i) {
        case 0:  for (auto [n, a] : {std::pair{1, 1.0}, {2, 0.7}, {3, 0.55}, {4, 0.45}, {6, 0.25}, {8, 0.3}}) harm(n, a); break;      // 16' 8' 5 1/3' 4' 2 2/3' 2'
        case 1:  for (auto [n, a] : {std::pair{1, 1.0}, {2, 0.8}, {4, 0.6}, {8, 0.5}, {16, 0.35}, {3, 0.3}, {6, 0.25}, {12, 0.2}}) harm(n, a); break;
        case 2:  for (int n = 1; n <= 64; ++n) harm(n, (n % 2 ? 1.0 : 0.25) / n * (1.0 + 2.0 * std::exp(-std::pow((n - 9) / 3.0, 2))), 0.0); break;
        case 3:  vowel({{5.0, 1.6}, {8.5, 2.0}, {18.0, 3.0}}); break;
        case 4:  vowel({{3.4, 1.2}, {13.5, 2.5}, {18.5, 3.0}}); break;
        case 5:  vowel({{2.2, 0.9}, {6.0, 1.5}, {17.0, 3.0}}); break;
        case 6:  for (auto [n, a] : {std::pair{1, 1.0}, {3, 0.35}, {7, 0.5}, {11, 0.4}, {17, 0.25}, {23, 0.15}}) harm(n, a, 1.2 * n); break;
        case 7:  for (auto [n, a] : {std::pair{1, 1.0}, {2, 0.6}, {5, 0.5}, {8, 0.45}, {12, 0.3}, {13, 0.3}, {19, 0.2}, {27, 0.12}}) harm(n, a, 0.7 * n); break;
        case 8:  for (int n = 1; n <= 40; ++n) harm(n, n % 2 ? 1.0 / n : 0.08 / n); break;                                       // nearly square, softened
        case 9:  for (int n = 1; n <= 80; ++n) harm(n, std::exp(-std::pow((n - 6.0) / 2.5, 2)) + 0.15 / n); break;
        case 10: cycle([](double p) { const double q = std::fmod(p * 1.5, 1.0); return 2.0 * q - 1.0; }); break;                   // hard sync, ratio 1.5
        case 11: cycle([](double p) { const double q = std::fmod(p * 2.5, 1.0); return (2.0 * q - 1.0) * (1.0 - 0.4 * p); }); break;
        case 12: cycle([](double p) { return std::sin(2 * kPi * p + 1.8 * std::sin(4 * kPi * p)); }); break;
        case 13: cycle([](double p) { return std::sin(2 * kPi * p + 2.6 * std::sin(6 * kPi * p)); }); break;
        case 14: cycle([](double p) { return std::sin(2 * kPi * p + 3.5 * std::sin(2 * kPi * p + 1.2 * std::sin(2 * kPi * p))); }); break;
        case 15: cycle([](double p) { return std::sin(kPi * 2.6 * std::sin(2 * kPi * p)); }); break;                              // wavefolded sine
        case 16: cycle([](double p) { return std::sin(kPi * 4.3 * std::sin(2 * kPi * p) + 0.3); }); break;
        case 17: cycle([](double p) { return std::tanh(5.0 * std::sin(2 * kPi * p)); }); break;
        case 18: cycle([](double p) { const double s = std::sin(2 * kPi * p); return s > 0 ? 2.0 * s - 0.64 : -0.64; }); break;
        case 19: cycle([](double p) { return (2 * p - 1) + 0.6 * (2 * std::fmod(2 * p + 0.13, 1.0) - 1); }); break;
        case 20: cycle([](double p) { return std::floor(p * 8.0) / 3.5 - 1.0; }); break;
        case 21: cycle([](double p) { const double d = p - 0.5; return 2.2 * std::exp(-d * d / 0.0012) - 0.15; }); break;
        case 22: harm(1, 1.0); harm(2, 0.55); harm(3, 0.2); harm(4, 0.08); break;
        case 23: for (int n = 1; n <= 120; ++n) harm(n, (1.0 / n) * (0.6 + 0.4 * std::cos(n * 0.9)) * (1.0 + 1.2 * std::exp(-std::pow((n - 14) / 6.0, 2))), rng.uni() * 0.4); break;
        case 24: vowel({{4.0, 2.5}, {8.0, 2.5}, {19.0, 4.0}, {24.0, 4.0}}); break;
        case 25: for (int n = 1; n <= 200; ++n) harm(n, (n < 6 ? 0.4 : 1.0) / std::pow(n, 0.6) * (rng.uni() > 0.55 ? 1.0 : 0.1), rng.uni() * 2 * kPi); break;
        case 26: for (int n = 1; n <= 24; ++n) harm(n, std::pow(0.82, n) * (n % 3 == 0 ? 1.6 : 0.6), rng.uni() * 2 * kPi); break;
        case 27: cycle([](double p) { return 0.6 * (2 * p - 1) + 0.6 * (p < 0.5 ? 1.0 : -1.0); }); break;
        case 28: for (int n = 1; n <= 96; ++n) harm(n, (1.0 / n) * (1.0 + 3.0 * std::exp(-std::pow((n - 2.0 * std::sqrt(double(n)) - 6) / 3.0, 2)))); break;
        case 29: for (auto [n, a] : {std::pair{1, 0.6}, {4, 1.0}, {9, 0.55}, {16, 0.35}, {25, 0.2}}) harm(n, a); break;
        case 30: for (int n = 1; n <= 20; ++n) harm(n, (n % 2 ? 1.0 : 0.35) * std::pow(0.6, n - 1)); break;                     // flue pipe
        case 31: cycle([](double p) { return std::round(std::sin(2 * kPi * p) * 3.0) / 3.0; }); break;                           // 3-bit sine
        default: harm(1, 1.0);
    }
    if (i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 15 || i == 16 || i == 17 || i == 18 || i == 19 || i == 20 ||
        i == 21 || i == 27 || i == 31)
        return;                        // built from a cycle above
    w.build(c, 1.0f);
}

}  // namespace td
