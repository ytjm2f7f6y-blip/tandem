// Tandem: the engine's tests. Each prints what it measured; a failed check makes the run fail.
//   cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
#include "td_engine.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>

using namespace td;

static const char* tp_modeName(int m) {
    static const char* n[] = {"UP", "DOWN", "UP+DOWN", "UP&DOWN", "RANDOM", "AS PLAYED", "CHORD"};
    return n[std::clamp(m, 0, 6)];
}

static int failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++failures;
}
static std::string fmt(const char* f, double a, double b = 0, double c = 0) {
    char s[256];
    std::snprintf(s, sizeof s, f, a, b, c);
    return s;
}

// A plain patch to start from: one sawtooth, filter open, no movement.
static LayerParams plain() {
    LayerParams p;
    p.drift = 0.0f;
    p.velVca = 0.0f;
    p.e2A = 0.0f; p.e2D = 0.5f; p.e2S = 1.0f; p.e2R = 0.2f;
    p.cutoff = 1.0f;
    p.resonance = 0.0f;
    p.lfo2Pitch = 0.0f;
    p.route = kRouteParallel;      // one ear: the left and right channels are the same
    p.parallelType = kP2LP2LP;
    return p;
}

static std::vector<float> renderNote(Engine& e, int note, double seconds, double rate, std::vector<float>* right = nullptr) {
    const int n = int(seconds * rate);
    std::vector<float> L(static_cast<size_t>(n)), R(static_cast<size_t>(n));
    e.noteOn(note, 1.0f);
    e.render(L.data(), R.data(), n);
    if (right) *right = R;
    return L;
}

// Frequency by counting rising zero crossings (with interpolation) after `skip` samples.
static double zeroCrossHz(const std::vector<float>& x, double rate, size_t skip) {
    double first = -1, last = -1;
    int count = 0;
    for (size_t i = skip + 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) {
            const double t = double(i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? rate * count / (last - first) : 0.0;
}

// Power spectrum (Blackman-Harris window), length a power of two.
static std::vector<double> spectrum(const std::vector<float>& x, size_t start, size_t n) {
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = 2 * kPi * double(i) / double(n);
        const double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
        a[i] = x[start + i] * w;
    }
    fft(a, -1);
    std::vector<double> p(n / 2);
    for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm(a[i]);
    return p;
}

static void testPitch() {
    std::printf("pitch\n");
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        Engine e;
        e.prepare(rate, 512);
        LayerParams p = plain();
        p.osc1Wave = kWSine;
        e.layer(0).setParams(p);
        auto x = renderNote(e, 69, 2.0, rate);
        const double hz = zeroCrossHz(x, rate, size_t(rate * 0.2));
        const double cents = 1200.0 * std::log2(hz / 440.0);
        check(std::abs(cents) < 0.05, fmt("A4 at %.0f Hz: %.4f Hz (%.4f cents)", rate, hz, cents));
    }
    {   // tune and fine
        Engine e; e.prepare(48000, 512);
        LayerParams p = plain(); p.osc1Wave = kWSine; p.osc1Tune = 7; p.osc1Fine = -25;
        e.layer(0).setParams(p);
        auto x = renderNote(e, 60, 2.0, 48000);
        const double want = 440.0 * std::pow(2.0, (60 + 7 - 0.25 - 69) / 12.0);
        const double hz = zeroCrossHz(x, 48000, 9600);
        check(std::abs(1200 * std::log2(hz / want)) < 0.05, fmt("C4 +7 semitones -25 cents: %.3f Hz (want %.3f)", hz, want));
    }
    {   // drone: OSC 2 holds its pitch whatever is played
        Engine e; e.prepare(48000, 512);
        LayerParams p = plain(); p.osc1Level = 0; p.osc2Level = 0.55f; p.osc2Wave = kWDroneSine;
        e.layer(0).setParams(p);
        auto x = renderNote(e, 84, 2.0, 48000);
        const double hz = zeroCrossHz(x, 48000, 9600);
        check(std::abs(hz - 65.406) < 0.05, fmt("drone sine stays on C2 when C6 is played: %.3f Hz", hz));
    }
}

static void testAliasing() {
    std::printf("aliasing (energy away from the harmonics, below 20 kHz)\n");
    struct Case { const char* name; int wave; int note; float detune; int sync; float tune2; };
    const Case cases[] = {{"saw, C7", kWSaw, 96, 0, 0, 0}, {"square, C7", kWSquare, 96, 0, 0, 0}, {"saw, E7", kWSaw, 100, 0, 0, 0},
                          {"alt wave (FM Growl), C6", kWAlt1, 84, 0, 0, 0},
                          {"sync 2-1, OSC 2 a fifth up + 5, C6", kWSaw, 84, 0, 1, 7}};
    for (const auto& cs : cases) {
        const double rate = 44100.0;
        Engine e; e.prepare(rate, 512);
        LayerParams p = plain();
        p.osc1Wave = cs.wave;
        p.osc1Alt[0] = 14;
        p.osc1Level = 0.5f;
        if (cs.sync) {           // listen to the synced OSC 2 only
            p.sync = 1; p.osc2Wave = kWSaw; p.osc2Tune = cs.tune2; p.osc2Fine = 50.0f * 0.0f; p.osc1Level = 0.0f; p.osc2Level = 0.5f;
        }
        e.layer(0).setParams(p);
        auto x = renderNote(e, cs.note, 1.5, rate);
        const size_t n = 32768;
        auto P = spectrum(x, 16384, n);
        double f0 = 440.0 * std::pow(2.0, (cs.note - 69) / 12.0);
        double harm = 0, other = 0;
        for (size_t k = 2; k < n / 2; ++k) {
            const double f = double(k) * rate / double(n);
            if (f > 20000.0) break;
            const double h = f / f0;
            const double dist = std::abs(h - std::round(h)) * f0;     // Hz from the nearest harmonic
            if (dist < 25.0) harm += P[k]; else other += P[k];
        }
        const double db = 10.0 * std::log10(other / harm + 1e-30);
        check(db < -60.0, std::string(cs.name) + fmt(": %.1f dB", db));
    }
}

static void testFilterCore() {
    std::printf("filter core (small signal, resonance 0, cutoff 1 kHz at 96 kHz)\n");
    const float fs = 96000.0f, fc = 1000.0f;
    const float g = tanPade(float(kPi) * fc / fs);
    auto gainAt = [&](int kind, float hz, float k) {
        FilterCore f;
        const Mix5 m = poleMix(kind);
        const int n = int(fs * 0.5f);
        double in = 0, out = 0;
        for (int i = 0; i < n; ++i) {
            const float x = 0.01f * std::sin(2.0f * float(kPi) * hz * float(i) / fs);
            const float y = f.process(x, g, k, 0.0f, m);
            if (i > n / 2) { in += x * x; out += y * y; }
        }
        return 10.0 * std::log10(out / in);
    };
    check(std::abs(gainAt(0, 1000, 0) + 12.04) < 0.3, fmt("4-pole low pass at cutoff: %.2f dB (ideal -12.04)", gainAt(0, 1000, 0)));
    check(std::abs(gainAt(0, 8000, 0) + 72.7) < 2.0, fmt("4-pole low pass 3 octaves up: %.1f dB (ideal -72.7)", gainAt(0, 8000, 0)));
    check(std::abs(gainAt(2, 1000, 0) + 3.01) < 0.2, fmt("1-pole low pass at cutoff: %.2f dB (ideal -3.01)", gainAt(2, 1000, 0)));
    check(std::abs(gainAt(3, 125, 0) + 54.2) < 2.0, fmt("3-pole high pass 3 octaves down: %.1f dB (ideal -54.2)", gainAt(3, 125, 0)));
    check(std::abs(gainAt(6, 1000, 0)) < 0.3, fmt("4-pole band pass peak: %.2f dB (ideal 0)", gainAt(6, 1000, 0)));
    double apMax = 0;
    for (float hz : {50.0f, 300.0f, 1000.0f, 4000.0f, 15000.0f}) apMax = std::max(apMax, std::abs(gainAt(8, hz, 0)));
    check(apMax < 0.1, fmt("4-pole all pass, worst level change 50 Hz..15 kHz: %.3f dB", apMax));
    check(gainAt(9, 414.2f, 0) < -30.0 && gainAt(9, 2414.2f, 0) < -30.0, fmt("phaser notches at 0.414 and 2.414 x cutoff: %.1f dB, %.1f dB", gainAt(9, 414.2f, 0), gainAt(9, 2414.2f, 0)));
    check(gainAt(0, 1000, 3.5f) > 0.0, fmt("4-pole low pass at cutoff, feedback 3.5: %+.1f dB (a resonant peak)", gainAt(0, 1000, 3.5f)));
    // Resonance type 1 keeps the low end, type 2 lets it drop by 1 / (1 + k).
    auto lowGain = [&](bool type1, float hz) {
        FilterCore f; const Mix5 m = poleMix(0); double in = 0, out = 0; const int n = int(fs);
        const float gg = type1 ? tanPade(float(kPi) * fc * kType1Tune / fs) : g;
        const float gh = type1 ? tanPade(float(kPi) * 0.25f * fc * kType1Tune / fs) : 0.0f;
        const float k = type1 ? 3.5f * kType1Gain : 3.5f;
        for (int i = 0; i < n; ++i) { const float x = 0.01f * std::sin(2.0f * float(kPi) * hz * float(i) / fs); const float y = f.process(x, gg, k, gh, m); if (i > n / 2) { in += x * x; out += y * y; } }
        return 10.0 * std::log10(out / in);
    };
    check(std::abs(lowGain(true, 60)) < 3.0 && lowGain(false, 60) < -12.0, fmt("60 Hz, resonance near the top: type 1 %+.1f dB, type 2 %+.1f dB", lowGain(true, 60), lowGain(false, 60)));
    check(std::abs(lowGain(true, 30)) < 3.0, fmt("30 Hz, type 1: %+.1f dB", lowGain(true, 30)));
}

static void testSelfOsc() {
    std::printf("self-oscillation, keyboard tracking ON\n");
    for (int type = 1; type >= 0; --type)
    for (int note : {48, 60, 72, 84}) {
        const double rate = 48000.0;
        Engine e; e.prepare(rate, 512);
        LayerParams p = plain();
        p.route = kRouteStereo; p.stereoType = kS4LP; p.resType = type;
        p.osc1Level = 0.0f;
        p.resonance = 1.0f;
        p.keytrack = 2;
        p.cutoff = float(std::log2(261.6256 / 16.0) / 10.3);     // the filter sings C4 at C4
        e.layer(0).setParams(p);
        auto x = renderNote(e, note, 3.0, rate);
        const double hz = zeroCrossHz(x, rate, size_t(rate * 1.5));
        const double want = 261.6256 * std::pow(2.0, (note - 60) / 12.0);
        double pk = 0; for (size_t i = size_t(rate); i < x.size(); ++i) pk = std::max(pk, double(std::abs(x[i])));
        const double cents = 1200.0 * std::log2(hz / want);
        check(std::abs(cents) < 25.0 && pk > 0.05, fmt("type %.0f, note %.0f: %.2f Hz", 2 - type, note, hz) + fmt(" (%.1f cents off), level %.2f", cents, pk));
    }
}

static void testEnvelopes() {
    std::printf("envelopes\n");
    const float fs = 48000.0f;
    for (float x : {0.0f, 0.3f, 0.6f}) {
        Env e;
        const float T = Env::attackTime(x);
        e.set(T, 1.0f, 1.0f, 1.0f, 0.0f, false, fs);
        e.gateOn();
        int n = 0;
        while (e.v < 0.999f && n < 10000000) { e.tick(1.0f / fs); ++n; }
        check(std::abs(n / fs - T) / T < 0.02, fmt("attack fader %.1f: %.4f s (set %.4f s)", x, n / fs, T));
    }
    {
        Env e;
        const float D = Env::decayTime(0.5f);
        e.set(Env::attackTime(0), D, 0.0f, D, 0.0f, false, fs);
        e.gateOn();
        while (e.st == Env::kAttack) e.tick(1.0f / fs);
        int n = 0;
        while (e.v > 0.01f) { e.tick(1.0f / fs); ++n; }
        check(std::abs(n / fs - D) / D < 0.02, fmt("decay fader 0.5 to 1%%: %.3f s (set %.3f s)", n / fs, D));
    }
    {   // loop mode cycles between sustain and full
        Env e;
        e.set(0.01f, 0.01f, 0.5f, 0.1f, 0.0f, true, fs);
        e.gateOn();
        float lo = 1, hi = 0;
        for (int i = 0; i < int(fs * 0.5f); ++i) { const float v = e.tick(1.0f / fs); if (i > fs * 0.1f) { lo = std::min(lo, v); hi = std::max(hi, v); } }
        check(hi > 0.99f && lo > 0.49f && lo < 0.52f, fmt("loop mode swings %.3f .. %.3f (sustain 0.5)", lo, hi));
    }
}

static void testVoices() {
    std::printf("voices\n");
    auto sounding = [](int route, int mode, int voices, int notes) {
        Engine e; e.prepare(48000, 512);
        LayerParams p = plain(); p.route = route; p.voiceMode = mode; p.voices = voices;
        e.layer(0).setParams(p);
        std::vector<float> L(480), R(480);
        for (int i = 0; i < notes; ++i) { e.noteOn(60 + i * 3, 0.8f); e.render(L.data(), R.data(), 480); }
        return e.layer(0).soundingVoices();
    };
    check(sounding(kRouteParallel, kPoly1, 4, 6) == 4, fmt("Poly 1, 4 voices, 6 notes: %.0f sounding", sounding(kRouteParallel, kPoly1, 4, 6)));
    check(sounding(kRouteStereo, kPoly1, 4, 6) == 2, fmt("binaural, 4 voices make 2 super voices: %.0f sounding", sounding(kRouteStereo, kPoly1, 4, 6)));
    check(sounding(kRouteParallel, kUnison, 4, 1) == 4, fmt("unison stacks all 4: %.0f sounding", sounding(kRouteParallel, kUnison, 4, 1)));
    check(sounding(kRouteParallel, kSolo, 8, 3) == 1, fmt("solo: %.0f sounding", sounding(kRouteParallel, kSolo, 8, 3)));
    {   // a stolen voice must not click: no jump bigger than the wave's own steps
        Engine e; e.prepare(48000, 64);
        LayerParams p = plain(); p.osc1Wave = kWSine; p.voices = 2; p.route = kRouteParallel;
        e.layer(0).setParams(p);
        std::vector<float> L(4800), R(4800);
        float maxStep = 0, prev = 0;
        for (int k = 0; k < 8; ++k) {
            e.noteOn(48 + k * 5, 1.0f);
            e.render(L.data(), R.data(), 4800);
            for (float v : L) { maxStep = std::max(maxStep, std::abs(v - prev)); prev = v; }
        }
        check(maxStep < 0.05f, fmt("stealing voices: largest step between samples %.4f", maxStep));
    }
    {   // legato: no new attack when notes overlap
        Engine e; e.prepare(48000, 64);
        LayerParams p = plain(); p.voiceMode = kLegato; p.e2A = 0.6f;
        e.layer(0).setParams(p);
        std::vector<float> L(48000), R(48000);
        e.noteOn(60, 1); e.render(L.data(), R.data(), 48000);
        double before = 0; for (int i = 47000; i < 48000; ++i) before = std::max(before, double(std::abs(L[size_t(i)])));
        e.noteOn(64, 1); e.render(L.data(), R.data(), 4800);
        double after = 0; for (int i = 0; i < 4800; ++i) after = std::max(after, double(std::abs(L[size_t(i)])));
        check(after > 0.8 * before, fmt("legato keeps the level through a new note: %.3f then %.3f", before, after));
    }
}

static void testGlide() {
    std::printf("portamento\n");
    Engine e; e.prepare(48000, 512);
    LayerParams p = plain(); p.osc1Wave = kWSine; p.voiceMode = kSolo; p.portamento = 1.0f;
    e.layer(0).setParams(p);
    std::vector<float> L(48000 * 3), R(48000 * 3);
    e.noteOn(48, 1); e.render(L.data(), R.data(), 4800);
    e.noteOn(60, 1); e.render(L.data(), R.data(), 48000 * 3);
    // 10 s per octave: after 2.5 .. 3.0 s the pitch is 3 to 3.6 semitones up.
    const std::vector<float> seg(L.begin() + 48000 * 2 + 24000, L.end());
    const double hz = zeroCrossHz(seg, 48000, 0);
    const double semis = 12 * std::log2(hz / (440.0 * std::pow(2.0, (48 - 69) / 12.0)));
    check(std::abs(semis - 3.3) < 0.2, fmt("full portamento (10 s an octave): %.2f semitones up at 2.75 s (want 3.3)", semis));
}

static void testDecimator() {
    std::printf("decimator (2x down)\n");
    auto gain = [](double hz) {
        Decimator2 d; const double fs2 = 88200; double in = 0, out = 0;
        for (int i = 0; i < 40000; ++i) {
            const float a = float(std::sin(2 * kPi * hz * (2 * i) / fs2)), b = float(std::sin(2 * kPi * hz * (2 * i + 1) / fs2));
            const float y = d.process(a, b);
            if (i > 2000) { in += 0.5; out += y * y; }
        }
        return 10 * std::log10(out / in);
    };
    check(std::abs(gain(1000)) < 0.01 && std::abs(gain(18000)) < 0.01, fmt("1 kHz %.4f dB, 18 kHz %.4f dB", gain(1000), gain(18000)));
    check(gain(30000) < -100 && gain(40000) < -100, fmt("30 kHz %.1f dB, 40 kHz %.1f dB (they would fold to 14 and 4 kHz)", gain(30000), gain(40000)));
}

static void testDelay() {
    std::printf("delay\n");
    Delay d; d.prepare(48000);
    LayerParams p; p.dlyDiv = 9; p.dlyFb = 0.0f; p.dlyDamp = 0.0f;
    std::vector<float> inL(48000, 0), inR(48000, 0), oL(48000, 0), oR(48000, 0);
    inL[100] = 1.0f;
    d.process(p, 120.0f, 0.0f, inL.data(), inR.data(), oL.data(), oR.data(), 48000);
    size_t at = 0; float pk = 0;
    for (size_t i = 0; i < oL.size(); ++i) if (std::abs(oL[i]) > pk) { pk = std::abs(oL[i]); at = i; }
    check(at == 100 + 24000, fmt("quarter note at 120 BPM: echo after %.0f samples (want 24000)", double(at) - 100));
}

static void testStability() {
    std::printf("stability (random settings, every routing and voice mode)\n");
    Rng r(1234);
    bool ok = true;
    double worst = 0;
    for (int t = 0; t < 60 && ok; ++t) {
        Engine e; e.prepare(t % 2 ? 44100 : 96000, 512);
        LayerParams p;
        p.osc1Wave = int(r.uni() * kOsc1Waves); p.osc2Wave = int(r.uni() * kOsc2Waves);
        p.osc1Tune = std::round(r.bi() * 7); p.osc2Tune = std::round(r.bi() * 7);
        p.sync = int(r.uni() * 3); p.modOsc = int(r.uni() * 3); p.detune = r.uni();
        p.waveSrc = int(r.uni() * 3); p.waveMod = r.uni(); p.pitchSrc = int(r.uni() * 3); p.pitchMod = r.uni();
        p.osc1Level = r.uni(); p.osc2Level = r.uni();
        p.cutoff = r.uni(); p.resonance = r.uni(); p.keytrack = int(r.uni() * 3); p.vcfSrc = int(r.uni() * 3); p.vcfMod = r.uni();
        p.deltaCutoff = r.bi(); p.route = int(r.uni() * 3); p.stereoType = int(r.uni() * kNumStereoTypes);
        p.parallelType = int(r.uni() * kNumParallelTypes); p.seriesType = int(r.uni() * kNumSeriesTypes); p.resType = int(r.uni() * 2);
        p.lfo1Wave = int(r.uni() * kLfo1Waves); p.lfo1Mode = int(r.uni() * 3); p.lfo1Rate = r.uni(); p.lfo1Phase = r.uni();
        p.e1Mode = int(r.uni() * 3); p.e1A = r.uni() * 0.4f; p.e1D = r.uni() * 0.6f; p.e1S = r.uni();
        p.vcaSrc = int(r.uni() * 3); p.e3Mode = int(r.uni() * 3);
        p.voiceMode = int(r.uni() * 5); p.unisonMode = int(r.uni() * 5); p.voices = 2 + 2 * int(r.uni() * 4);
        p.drift = r.uni(); p.panSpread = r.uni(); p.portamento = r.uni() * 0.5f;
        for (int k = 0; k < kMatrixSlots; ++k) { p.mSrc[k] = int(r.uni() * kNumModSources); p.mDst[k] = int(r.uni() * kNumModDests); p.mAmt[k] = r.bi(); p.mVia[k] = int(r.uni() * kNumModSources); p.mCrv[k] = int(r.uni() * 4); }
        p.vary = r.uni(); p.varySeed = int(r.next() % 1000u); p.rndCount = int(r.uni() * 9);
        p.drvMode = int(r.uni() * 4); p.drvAmt = r.uni(); p.chOn = r.uni() > 0.5f; p.eqLow = r.bi() * 15; p.eqMid = r.bi() * 15; p.eqHigh = r.bi() * 15;
        p.fxSend = r.uni(); p.fxBal = r.uni(); p.dlyFb = r.uni(); p.dlyMode = int(r.uni() * 3); p.rvSize = r.uni(); p.freeze = r.uni() > 0.8f;
        e.layer(0).setParams(p);
        e.modWheel(r.uni()); e.aftertouch(r.uni()); e.bend(r.bi());
        std::vector<float> L(4096), R(4096);
        for (int b = 0; b < 40; ++b) {
            if (b % 5 == 0) e.noteOn(36 + int(r.uni() * 60), r.uni());
            if (b % 7 == 3) e.noteOff(36 + int(r.uni() * 60));
            e.render(L.data(), R.data(), 4096);
            for (int i = 0; i < 4096; ++i) {
                if (!std::isfinite(L[size_t(i)]) || !std::isfinite(R[size_t(i)])) { ok = false; break; }
                worst = std::max(worst, double(std::max(std::abs(L[size_t(i)]), std::abs(R[size_t(i)]))));
            }
        }
    }
    check(ok && worst < 8.0, fmt("60 random patches: all finite, loudest sample %.2f", worst));
}

static void testCost() {
    std::printf("cost (one layer, 44.1 kHz, notes held, effects on)\n");
    struct Case { const char* name; int route; int voices; float detune; int modOsc; };
    const Case cases[] = {{"hardware: 4 voices, parallel, no detune", kRouteParallel, 4, 0.0f, 1},
                          {"hardware: 2 super voices (binaural), detune both", kRouteStereo, 4, 0.6f, 1},
                          {"4 voices, parallel, detune both (7 sisters each)", kRouteParallel, 4, 0.6f, 1},
                          {"16 voices, parallel, detune both", kRouteParallel, 16, 0.6f, 1}};
    for (const auto& cs : cases) {
        Engine e; e.prepare(44100, 512);
        LayerParams p = plain(); p.route = cs.route; p.voices = cs.voices; p.detune = cs.detune; p.modOsc = cs.modOsc;
        p.osc2Level = 0.4f; p.osc2Wave = kWSquare; p.resonance = 0.5f; p.cutoff = 0.7f; p.chOn = true; p.fxSend = 0.3f; p.drvMode = kDriveHarmonic;
        e.layer(0).setParams(p);
        for (int i = 0; i < 16; ++i) e.noteOn(48 + i * 2, 0.8f);
        std::vector<float> L(512), R(512);
        const int blocks = int(44100.0 * 10 / 512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.render(L.data(), R.data(), 512);
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  %-52s %5.1f%% of one core\n", cs.name, 100.0 * secs / 10.0);
    }
}

// The matrix: twelve slots, a VIA source scaling the amount, and the curves.
static void testMatrix() {
    std::printf("matrix\n");
    auto modAt = [](LayerParams p, float vel, float wheel) {
        Engine e; e.prepare(48000, 512);
        e.layer(0).setParams(p);
        e.modWheel(wheel);
        std::vector<float> L(512), R(512);
        e.noteOn(60, vel);
        e.render(L.data(), R.data(), 512);
        return e.layer(0).modNow(kMdCutoff);
    };
    LayerParams p = plain();
    p.mSrc[11] = kMsVelocity; p.mDst[11] = kMdCutoff; p.mAmt[11] = 0.5f; p.mVia[11] = kMsModWheel;
    const float none = modAt(p, 0.7f, 0.0f), full = modAt(p, 0.7f, 1.0f), half = modAt(p, 0.7f, 0.5f);
    check(std::abs(none) < 1e-6f && std::abs(full - 0.35f) < 1e-4f && std::abs(half - 0.175f) < 1e-4f,
          fmt("slot 12, velocity 0.7 x 0.5 via the wheel: %.4f at 0, %.4f at half, %.4f at full", none, half, full));
    p.mVia[11] = kMsNone; p.mCrv[11] = kCurveExp;
    const float ex = modAt(p, 0.7f, 0.0f);
    check(std::abs(ex - 0.245f) < 1e-4f, fmt("EXP squares the source: %.4f (want 0.2450)", ex));
    p.mCrv[11] = kCurveStep;
    const float st = modAt(p, 0.7f, 0.0f);
    check(std::abs(st - 4.0f / 12.0f) < 1e-4f, fmt("STEP rounds to twelfths: 0.35 becomes %.4f (want 0.3333)", st));
    // SMOOTH: a source that jumps (the wheel) arrives over tens of milliseconds, not at once.
    {
        LayerParams q = plain();
        q.mSrc[0] = kMsModWheel; q.mDst[0] = kMdCutoff; q.mAmt[0] = 1.0f; q.mCrv[0] = kCurveSmooth;
        Engine e; e.prepare(48000, 64);
        e.layer(0).setParams(q);
        std::vector<float> L(64), R(64);
        e.noteOn(60, 1.0f);
        for (int i = 0; i < 20; ++i) e.render(L.data(), R.data(), 64);
        e.modWheel(1.0f);
        e.render(L.data(), R.data(), 64);
        const float early = e.layer(0).modNow(kMdCutoff);
        for (int i = 0; i < 200; ++i) e.render(L.data(), R.data(), 64);
        const float late = e.layer(0).modNow(kMdCutoff);
        check(early > 0.0f && early < 0.1f && late > 0.98f, fmt("SMOOTH: the wheel thrown to full gives %.3f after 1.3 ms, %.3f after 268 ms", early, late));
    }
    // PITCH: a slot of +1/12 in steps raises the note a semitone.
    {
        LayerParams q = plain();
        q.mSrc[5] = kMsVelocity; q.mDst[5] = kMdPitch; q.mAmt[5] = 1.0f / 12.0f; q.mCrv[5] = kCurveStep;
        Engine e; e.prepare(48000, 512);
        e.layer(0).setParams(q);
        const auto x = renderNote(e, 69, 0.5, 48000);
        const double hz = zeroCrossHz(x, 48000, 4800);
        check(std::abs(1200 * std::log2(hz / 466.164)) < 1.0, fmt("pitch destination, +1 semitone on A4: %.2f Hz (want 466.16)", hz));
    }
    // Removing the last slot leaves no modulation behind.
    {
        Engine e; e.prepare(48000, 512);
        LayerParams q = plain();
        q.mSrc[0] = kMsVelocity; q.mDst[0] = kMdCutoff; q.mAmt[0] = 1.0f;
        e.layer(0).setParams(q);
        std::vector<float> L(512), R(512);
        e.noteOn(60, 1.0f);
        e.render(L.data(), R.data(), 512);
        q.mSrc[0] = kMsNone;
        e.layer(0).setParams(q);
        e.render(L.data(), R.data(), 512);
        e.render(L.data(), R.data(), 512);
        check(e.layer(0).modNow(kMdCutoff) == 0.0f, fmt("emptying the matrix clears its output: %.3f", e.layer(0).modNow(kMdCutoff)));
    }
}

// NOTE RND: a value per note; with a count, a cycle that long, again from the top after a pause.
static void testNoteRnd() {
    std::printf("note random\n");
    auto run = [](int count, int notes, std::vector<float>& out, bool pause = false) {
        Engine e; e.prepare(48000, 256);
        LayerParams p = plain();
        p.rndCount = count;
        p.mSrc[0] = kMsNoteRnd; p.mDst[0] = kMdCutoff; p.mAmt[0] = 1.0f;
        e.layer(0).setParams(p);
        std::vector<float> L(256), R(256);
        for (int k = 0; k < notes; ++k) {
            if (pause && k == notes / 2) for (int i = 0; i < 300; ++i) e.render(L.data(), R.data(), 256);     // 1.6 s, no keys
            e.noteOn(60 + k, 1.0f);
            e.render(L.data(), R.data(), 256);
            out.push_back(e.layer(0).modNow(kMdCutoff));
            e.noteOff(60 + k);
            e.render(L.data(), R.data(), 256);
        }
    };
    std::vector<float> a, b, c;
    run(3, 7, a);
    check(a[0] == a[3] && a[1] == a[4] && a[2] == a[5] && a[0] != a[1] && a[1] != a[2],
          fmt("count 3 cycles: %.3f %.3f %.3f, then", a[0], a[1], a[2]) + fmt(" %.3f %.3f ...", a[3], a[4]));
    run(0, 12, b);
    int distinct = 0;
    for (size_t i = 1; i < b.size(); ++i) distinct += b[i] != b[i - 1];
    float lo = 1, hi = -1;
    for (float v : b) { lo = std::min(lo, v); hi = std::max(hi, v); }
    check(distinct == 11 && lo >= -1.0f && hi <= 1.0f, fmt("free: 12 notes, %.0f changes, range %.2f .. %.2f", distinct, lo, hi));
    run(4, 6, c, true);
    check(c[3] == c[0] && c[4] == c[1], fmt("a pause starts the cycle again: %.3f after the pause, %.3f at the start", c[3], c[0]));
}

// VARY: nothing at 0; at 1, notes differ, the same way each time the phrase is played after a pause.
static void testVary() {
    std::printf("vary\n");
    auto phrase = [](float vary, int seed, std::vector<float>& cut, std::vector<int>& notes) {
        Engine e; e.prepare(48000, 256);
        LayerParams p = plain();
        p.voices = 8; p.vary = vary; p.varySeed = seed;
        e.layer(0).setParams(p);
        std::vector<float> L(256), R(256);
        for (int rep = 0; rep < 2; ++rep) {
            for (int k = 0; k < 8; ++k) {
                e.noteOn(48 + 3 * k, 0.8f);
                e.render(L.data(), R.data(), 256);
                const Voice* v = nullptr;
                for (int i = 0; i < 8; ++i) { const Voice& w = e.layer(0).voice(i); if (w.active && w.gate && w.key == 48 + 3 * k) v = &w; }
                cut.push_back(v ? v->varyCut + 100.0f * (v->vel - 0.8f) + 1000.0f * (v->varyEnvK - 1.0f) : -999.0f);
                notes.push_back(v ? v->note - v->key : -99);
                e.noteOff(48 + 3 * k);
                e.render(L.data(), R.data(), 256);
            }
            for (int i = 0; i < 400; ++i) e.render(L.data(), R.data(), 256);      // a pause
        }
    };
    std::vector<float> c0, c1, c2;
    std::vector<int> n0, n1, n2;
    phrase(0.0f, 7, c0, n0);
    bool flat = true;
    for (size_t i = 0; i < c0.size(); ++i) flat = flat && c0[i] == 0.0f && n0[i] == 0;
    check(flat, "VARY at 0: every note exactly as played");
    phrase(1.0f, 7, c1, n1);
    bool same = true, varied = false;
    for (size_t i = 0; i < 8; ++i) { same = same && c1[i] == c1[i + 8] && n1[i] == n1[i + 8]; varied = varied || c1[i] != c1[0]; }
    int jumps = 0;
    for (int n : n1) jumps += n != 0;
    check(same && varied, fmt("VARY at full: notes vary, and the phrase repeats the same after a pause (%.0f octave jumps in 16 notes)", jumps));
    phrase(1.0f, 8, c2, n2);
    bool differ = false;
    for (size_t i = 0; i < 8; ++i) differ = differ || c2[i] != c1[i];
    check(differ, "a new DICE roll gives a different pattern");
    // Octave jumps release cleanly: the key that went up stops its voice, whatever note it played.
    Engine e; e.prepare(48000, 256);
    LayerParams p = plain(); p.voices = 8; p.vary = 1.0f; p.e2R = 0.0f;
    std::vector<float> L(256), R(256);
    int stuck = 0;
    for (int seed = 0; seed < 30; ++seed) {
        p.varySeed = seed;
        e.layer(0).setParams(p);
        for (int k = 0; k < 16; ++k) e.noteOn(40 + k, 1.0f);
        e.render(L.data(), R.data(), 256);
        for (int k = 0; k < 16; ++k) e.noteOff(40 + k);
        for (int i = 0; i < 40; ++i) e.render(L.data(), R.data(), 256);
        stuck += e.layer(0).soundingVoices();
    }
    check(stuck == 0, fmt("30 rolls, 16 notes each, all let go: %.0f voices left sounding", stuck));
}


// ---------------------------------------------------------------------------------------
// Stage 2: the play modes, the arpeggiators, the external input.

// Which layers a note went to: bit 0 layer 1, bit 1 layer 2 (by their gated voices).
static int gatedMask(Engine& e) {
    int m = 0;
    for (int L = 0; L < 2; ++L)
        for (int i = 0; i < Layer::kMaxVoices; ++i)
            if (e.layer(L).voice(i).active && e.layer(L).voice(i).gate) { m |= 1 << L; break; }
    return m;
}
static int gatedCount(Engine& e, int L) {
    int n = 0;
    for (int i = 0; i < Layer::kMaxVoices; ++i) n += e.layer(L).voice(i).active && e.layer(L).voice(i).gate;
    return n;
}
static void twoPlain(Engine& e) {
    LayerParams p = plain();
    p.voices = 8;
    p.e2R = 0.0f;
    e.layer(0).setParams(p);
    e.layer(1).setParams(p);
}

static void testPlayModes() {
    std::printf("play modes\n");
    std::vector<float> L(256), R(256);
    auto fresh = [&](Engine& e, int mode) {
        e.prepare(48000, 256);
        twoPlain(e);
        PerfParams pp; pp.playMode = mode;
        e.setPerf(pp);
    };
    auto tick = [&](Engine& e, int blocks = 1) { for (int i = 0; i < blocks; ++i) e.render(L.data(), R.data(), 256); };
    {   // SINGLE: the edited layer alone; a key goes up on the layer it went down on
        Engine e; fresh(e, kPlaySingle);
        e.noteOn(60, 1); tick(e);
        const int a = gatedMask(e);
        PerfParams pp = e.perf(); pp.layer = 1; e.setPerf(pp);
        e.noteOn(64, 1); tick(e);
        const int b = gatedCount(e, 1);
        e.noteOff(60); tick(e, 4);
        check(a == 1 && b == 1 && gatedCount(e, 0) == 0 && gatedCount(e, 1) == 1,
              "SINGLE: the edited layer plays; a key let go after switching layers still lets its own note go");
    }
    {   // DUAL
        Engine e; fresh(e, kPlayDual);
        e.noteOn(60, 1); tick(e);
        check(gatedCount(e, 0) == 1 && gatedCount(e, 1) == 1, "DUAL: every note on both layers");
    }
    {   // SPLIT
        Engine e; fresh(e, kPlaySplit);
        PerfParams pp = e.perf(); pp.split = 60; e.setPerf(pp);
        e.noteOn(59, 1); tick(e);
        const int lo = gatedMask(e);
        e.noteOff(59); tick(e, 4);
        e.noteOn(60, 1); tick(e);
        check(lo == 2 && gatedMask(e) == 1, "SPLIT at C3: B2 on layer 2, C3 on layer 1");
    }
    {   // ONE-TWO
        Engine e; fresh(e, kPlayOneTwo);
        e.noteOn(48, 1); tick(e);
        e.noteOn(64, 1); e.noteOn(67, 1); tick(e);
        const bool first = gatedCount(e, 0) == 1 && gatedCount(e, 1) == 2;
        e.noteOff(48); e.noteOff(64); e.noteOff(67); tick(e, 4);
        e.noteOn(50, 1); tick(e);
        check(first && gatedCount(e, 0) == 1 && gatedCount(e, 1) == 0, "ONE-TWO: the first key on layer 1, the keys played with it on layer 2");
    }
    {   // ROTATE
        Engine e; fresh(e, kPlayRotate);
        int pattern = 0;
        for (int k = 0; k < 4; ++k) {
            e.noteOn(60 + k, 1); tick(e);
            pattern = pattern * 10 + gatedMask(e);
            e.noteOff(60 + k); tick(e, 4);
        }
        check(pattern == 1212, fmt("ROTATE: layers in turn (%.0f)", pattern));
    }
    {   // RANDOM: the odds
        int n1[3] = {0, 0, 0};
        const int odds[3] = {6, 0, 3};
        for (int t = 0; t < 3; ++t) {
            Engine e; fresh(e, kPlayRandom);
            PerfParams pp = e.perf(); pp.layerOdds = odds[t]; e.setPerf(pp);
            for (int k = 0; k < 200; ++k) {
                e.noteOn(60, 1); tick(e);
                n1[t] += gatedMask(e) == 1;
                e.noteOff(60); tick(e, 2);
            }
        }
        check(n1[0] == 200 && n1[1] == 0 && n1[2] > 70 && n1[2] < 130,
              fmt("RANDOM: layer 1 at 6/6, 0/6 and 3/6 of the time: %.0f, %.0f, %.0f of 200", n1[0], n1[1], n1[2]));
    }
    {   // CHAOS: the intervals
        Engine e; fresh(e, kPlayChaos);
        PerfParams pp = e.perf(); pp.fifthUp = 6; pp.octUp = 6; pp.layerOdds = 6; e.setPerf(pp);
        e.noteOn(48, 1); tick(e);
        int played = -1;
        for (int i = 0; i < Layer::kMaxVoices; ++i) if (e.layer(0).voice(i).gate) played = e.layer(0).voice(i).note;
        e.noteOff(48); tick(e, 4);
        pp.fifthUp = 0; pp.fifthDown = 3; pp.octUp = 0; pp.octDown = 0; e.setPerf(pp);
        int down = 0, same = 0;
        for (int k = 0; k < 120; ++k) {
            e.noteOn(60, 1); tick(e);
            for (int i = 0; i < Layer::kMaxVoices; ++i) if (e.layer(0).voice(i).gate) { down += e.layer(0).voice(i).note == 53; same += e.layer(0).voice(i).note == 60; }
            e.noteOff(60); tick(e, 2);
        }
        check(played == 48 + 19 && gatedCount(e, 0) == 0 && down > 35 && down < 85 && down + same == 120,
              fmt("CHAOS: a fifth and an octave up at 6/6 (played %.0f); a fifth down half the time at 3/6 (%.0f of 120), and every key let go", played, down));
    }
    {   // CASCADE: layer 1 heard only through layer 2's filter, at layer 2's EXT level
        auto loud = [&](float ext) {
            Engine e; e.prepare(48000, 256);
            LayerParams a = plain(); a.voices = 4;
            LayerParams b = plain(); b.voices = 4; b.osc1Level = 0.0f; b.osc2Level = 0.0f; b.extLevel = ext;
            e.layer(0).setParams(a); e.layer(1).setParams(b);
            PerfParams pp; pp.playMode = kPlayCascade; e.setPerf(pp);
            e.noteOn(57, 1);
            double sum = 0;
            for (int i = 0; i < 40; ++i) { e.render(L.data(), R.data(), 256); for (float x : L) sum += double(x) * x; }
            return std::sqrt(sum / (40 * 256));
        };
        const double off = loud(0.0f), on = loud(0.6f);
        check(off < 1e-5 && on > 0.02, fmt("CASCADE: layer 2's EXT at 0: silence (%.6f); at 6: layer 1 through it (%.3f)", off, on));
    }
    {   // key ranges, and MIDI channels
        Engine e; fresh(e, kPlayDual);
        PerfParams pp = e.perf(); pp.lo[0] = 60; pp.hi[1] = 59; e.setPerf(pp);
        e.noteOn(40, 1); tick(e);
        const int a = gatedMask(e);
        e.noteOff(40); tick(e, 4);
        pp = PerfParams{}; pp.playMode = kPlayDual; pp.chan[0] = 1; pp.chan[1] = 2; e.setPerf(pp);
        e.noteOn(60, 1, 2); tick(e);
        const int b = gatedMask(e);
        e.noteOn(60, 1, 1); tick(e);
        e.noteOff(60, 2); tick(e, 4);
        check(a == 2 && b == 2 && gatedMask(e) == 1, "key ranges keep notes to their layer; each layer hears its own MIDI channel, the same key on both");
    }
    {   // an unused layer sleeps
        Engine e; e.prepare(48000, 256);
        twoPlain(e);
        tick(e);
        const bool before = e.layer(1).asleep();
        e.noteOn(60, 1); tick(e);
        e.noteOff(60);
        for (int i = 0; i < 600; ++i) tick(e);         // 3.2 s
        check(before && e.layer(1).asleep() && e.layer(0).asleep(), "a layer with nothing to play sleeps (layer 2 in SINGLE; layer 1 after its tail)");
    }
}

// The arpeggiator: what it plays, and when. Reads the newest voice every 32 samples.
struct ArpRun {
    std::vector<int> notes;     // every note started, in order (chord steps: several at one time)
    std::vector<int> at;        // the sample each started at
    std::vector<int> ends;      // when each was let go (or -1)
};
static ArpRun watchArp(Engine& e, int layer, int samples, const std::function<void(Engine&, int)>& atSample = nullptr) {
    ArpRun r;
    std::vector<float> L(32), R(32);
    uint64_t lastAge[Layer::kMaxVoices] = {};
    bool wasGate[Layer::kMaxVoices] = {};
    int idx[Layer::kMaxVoices];
    for (int& i : idx) i = -1;
    for (int t = 0; t < samples; t += 32) {
        if (atSample) atSample(e, t);
        e.render(L.data(), R.data(), 32);
        for (int i = 0; i < Layer::kMaxVoices; ++i) {
            const Voice& v = e.layer(layer).voice(i);
            if (v.active && v.gate && v.age != lastAge[i]) {
                lastAge[i] = v.age;
                r.notes.push_back(v.note); r.at.push_back(t); r.ends.push_back(-1);
                idx[i] = int(r.notes.size()) - 1;
            }
            if (wasGate[i] && !v.gate && idx[i] >= 0 && r.ends[size_t(idx[i])] < 0) r.ends[size_t(idx[i])] = t;
            wasGate[i] = v.active && v.gate;
        }
    }
    return r;
}
static std::string seq(const ArpRun& r, size_t n = 12) {
    std::string s;
    for (size_t i = 0; i < r.notes.size() && i < n; ++i) s += (i ? " " : "") + std::to_string(r.notes[i]);
    return s;
}

static void testArp() {
    std::printf("arpeggiator\n");
    // 120 BPM, 1/16: a step every 6000 samples at 48 kHz.
    auto setup = [](Engine& e, int mode, int range = 1, float gate = 0.5f, float swing = 0.0f, bool hold = false) {
        e.prepare(48000, 256);
        LayerParams p = plain(); p.voices = 8; p.e2R = 0.0f;
        p.arpOn = true; p.arpMode = mode; p.arpRange = range; p.arpDiv = 4; p.arpGate = gate; p.arpSwing = swing; p.arpHold = hold;
        e.layer(0).setParams(p);
        PerfParams pp; pp.hostSync = false; pp.tempo = 120.0f;
        e.setPerf(pp);
        e.setTempo(120.0f);
    };
    auto chord = [](Engine& e) { e.noteOn(64, 0.8f); e.noteOn(60, 0.8f); e.noteOn(67, 0.8f); };
    {
        Engine e; setup(e, kArpUp);
        chord(e);
        const ArpRun r = watchArp(e, 0, 48000);
        bool timed = r.at.size() >= 8;
        for (size_t i = 0; i < r.at.size() && i < 8; ++i) timed = timed && std::abs(r.at[i] - int(i) * 6000) <= 32;
        bool gated = !r.ends.empty();
        for (size_t i = 0; i + 1 < r.ends.size(); ++i) gated = gated && std::abs(r.ends[i] - r.at[i] - 3000) <= 64;
        check(seq(r, 8) == "60 64 67 60 64 67 60 64" && timed && gated,
              "UP at 1/16, 120 BPM: " + seq(r, 8) + ", a step every 6000 samples, each held half a step");
    }
    const struct { int mode, range; const char* want; } orders[] = {
        {kArpDown, 1, "67 64 60 67 64 60"}, {kArpUpDown, 1, "60 64 67 64 60 64"}, {kArpUpDown2, 1, "60 64 67 67 64 60"},
        {kArpOrder, 1, "64 60 67 64 60 67"}, {kArpUp, 2, "60 64 67 72 76 79"}};
    for (const auto& o : orders) {
        Engine e; setup(e, o.mode, o.range);
        chord(e);
        const ArpRun r = watchArp(e, 0, 36000);
        check(seq(r, 6) == o.want, std::string("mode ") + tp_modeName(o.mode) + (o.range > 1 ? ", 2 octaves" : "") + ": " + seq(r, 6));
    }
    {
        Engine e; setup(e, kArpChord, 2);
        chord(e);
        const ArpRun r = watchArp(e, 0, 13000);
        check(seq(r, 9) == "60 64 67 72 76 79 60 64 67" || (r.notes.size() >= 6 && r.at[0] == r.at[2] && r.at[3] == r.at[5] && r.at[3] - r.at[0] >= 5900),
              "CHORD, 2 octaves: the chord a step, an octave up the next: " + seq(r, 9));
    }
    {
        Engine e; setup(e, kArpRandom);
        chord(e);
        const ArpRun r = watchArp(e, 0, 48000 * 4);
        int repeats = 0, counts[3] = {0, 0, 0};
        for (size_t i = 0; i < r.notes.size(); ++i) {
            if (i && r.notes[i] == r.notes[i - 1]) ++repeats;
            counts[r.notes[i] == 60 ? 0 : (r.notes[i] == 64 ? 1 : 2)]++;
        }
        check(repeats == 0 && counts[0] > 5 && counts[1] > 5 && counts[2] > 5,
              fmt("RANDOM: never the same note twice running; each note %.0f, %.0f, %.0f times", counts[0], counts[1], counts[2]) + fmt(" of %.0f", double(r.notes.size())));
    }
    {   // swing: the off steps late by a third of a step at full swing
        Engine e; setup(e, kArpUp, 1, 0.3f, 1.0f);
        chord(e);
        const ArpRun r = watchArp(e, 0, 30000);
        check(r.at.size() >= 4 && std::abs(r.at[1] - 8000) <= 32 && std::abs(r.at[2] - 12000) <= 32 && std::abs(r.at[3] - 20000) <= 32,
              fmt("full swing: steps at 0, %.0f, %.0f, %.0f samples (8000, 12000, 20000)", r.at.size() > 1 ? r.at[1] : -1, r.at.size() > 2 ? r.at[2] : -1, r.at.size() > 3 ? r.at[3] : -1));
    }
    {   // without HOLD the arpeggio stops when the keys come up; with it, it plays on until the next chord
        Engine e; setup(e, kArpUp);
        chord(e);
        const ArpRun r = watchArp(e, 0, 30000, [](Engine& en, int t) { if (t == 13000 - 13000 % 32) { en.noteOff(60); en.noteOff(64); en.noteOff(67); } });
        Engine h; setup(h, kArpUp, 1, 0.5f, 0.0f, true);
        chord(h);
        const ArpRun rh = watchArp(h, 0, 48000, [](Engine& en, int t) {
            if (t == 13000 - 13000 % 32) { en.noteOff(60); en.noteOff(64); en.noteOff(67); }
            if (t == 30016) { en.noteOn(50, 0.8f); }
        });
        int after = 0;
        for (size_t i = 0; i < rh.notes.size(); ++i) if (rh.at[i] > 30016) after += rh.notes[i] == 50 ? 1 : 100;
        check(r.notes.size() == 3 && rh.notes.size() >= 7 && after >= 2 && after < 100 && gatedCount(e, 0) == 0,
              fmt("keys up: the arpeggio stops (%.0f notes); with HOLD it plays on, then a new key replaces the chord (%.0f notes)", double(r.notes.size()), double(rh.notes.size())));
    }
    {   // on the host's grid: a key a little late waits for the next step; a loop back does not stall it
        Engine e; setup(e, kArpUp);
        PerfParams pp = e.perf(); pp.hostSync = true; e.setPerf(pp);
        double ppq = 10.1;
        const ArpRun r = watchArp(e, 0, 48000, [&](Engine& en, int t) {
            if (t == 0) { en.setTransport(true, ppq, 120.0); chord(en); }
            else if (t % 256 == 0) {
                ppq += 256.0 / 24000.0;
                if (t == 24064) ppq = 2.0;                  // the host loops back
                en.setTransport(true, ppq, 120.0);
            }
        });
        // 10.1 beats: 0.1 past a step (40 % of one), so the first waits for 10.25 = 0.15 beat = 3600 samples.
        bool afterLoop = false;
        for (size_t i = 0; i < r.at.size(); ++i) afterLoop = afterLoop || (r.at[i] > 24064 && r.at[i] < 24064 + 6100);
        check(!r.at.empty() && std::abs(r.at[0] - 3600) <= 32 && afterLoop,
              fmt("on the host's beat: the first step waits for the grid (%.0f samples, 3600); after a loop back the next comes within a step", r.at.empty() ? -1 : r.at[0]));
    }
    {   // the arpeggiator going off with keys down: they sound as played; HOLD alone keeps notes after the keys come up
        Engine e; setup(e, kArpUp);
        chord(e);
        std::vector<float> L(256), R(256);
        for (int i = 0; i < 10; ++i) e.render(L.data(), R.data(), 256);
        LayerParams p = e.layer(0).params(); p.arpOn = false; e.layer(0).setParams(p);
        for (int i = 0; i < 4; ++i) e.render(L.data(), R.data(), 256);
        const int held = gatedCount(e, 0);
        p.arpHold = true; e.layer(0).setParams(p);
        e.render(L.data(), R.data(), 256);
        e.noteOff(60); e.noteOff(64); e.noteOff(67);
        for (int i = 0; i < 4; ++i) e.render(L.data(), R.data(), 256);
        const int kept = gatedCount(e, 0);
        e.noteOn(72, 0.8f);
        for (int i = 0; i < 4; ++i) e.render(L.data(), R.data(), 256);
        const int fresh = gatedCount(e, 0);
        p.arpHold = false; e.layer(0).setParams(p);
        e.noteOff(72);
        for (int i = 0; i < 4; ++i) e.render(L.data(), R.data(), 256);
        check(held == 3 && kept == 3 && fresh == 1 && gatedCount(e, 0) == 0,
              fmt("ARP off: the chord held sounds (%.0f); HOLD keeps it after the keys (%.0f), a new key takes over (%.0f)", held, kept, fresh));
    }
    {   // each layer its own arpeggio, on the same grid
        Engine e; setup(e, kArpUp);
        LayerParams p = e.layer(0).params(); p.arpDiv = 3; e.layer(1).setParams(p);   // layer 2 in eighths
        PerfParams pp = e.perf(); pp.playMode = kPlayDual; e.setPerf(pp);
        chord(e);
        const ArpRun a = watchArp(e, 0, 24000);
        Engine e2; setup(e2, kArpUp);
        e2.layer(1).setParams(p);
        e2.setPerf(pp);
        chord(e2);
        const ArpRun b = watchArp(e2, 1, 24000);
        check(a.notes.size() == 4 && b.notes.size() == 2 && b.at.size() == 2 && std::abs(b.at[1] - 12000) <= 32,
              fmt("DUAL, two arpeggios: layer 1 in sixteenths (%.0f notes), layer 2 in eighths (%.0f), together", double(a.notes.size()), double(b.notes.size())));
    }
}

static void testExtIn() {
    std::printf("external input\n");
    const int n = 256;
    std::vector<float> L(n), R(n), xL(n), xR(n);
    Rng r(5);
    auto burst = [&](bool on) { for (int i = 0; i < n; ++i) { xL[size_t(i)] = on ? 0.5f * r.bi() : 0.0f; xR[size_t(i)] = xL[size_t(i)]; } };
    {   // the follower
        Engine e; e.prepare(48000, n);
        PerfParams pp; pp.efAttack = 0.005f; pp.efRelease = 0.1f; e.setPerf(pp);
        e.setExtConnected(true);
        float peak = 0.0f;
        for (int b = 0; b < 40; ++b) { burst(true); e.render(L.data(), R.data(), n, xL.data(), xR.data()); peak = std::max(peak, e.follower()); }
        for (int b = 0; b < 190; ++b) { burst(false); e.render(L.data(), R.data(), n, xL.data(), xR.data()); }
        check(peak > 0.35f && e.follower() < 0.02f, fmt("the follower rises with the input (%.2f) and falls away after it (%.3f after 1 s)", peak, e.follower()));
    }
    {   // VCA ON/EXT: the input opens the VCA
        Engine e; e.prepare(48000, n);
        LayerParams p = plain(); p.vcaSrc = 2; e.layer(0).setParams(p);
        PerfParams pp; pp.efAttack = 0.002f; pp.efRelease = 0.05f; e.setPerf(pp);
        e.setExtConnected(true);
        e.noteOn(57, 1);
        auto rms = [&](bool on, int blocks) {
            double s = 0;
            for (int b = 0; b < blocks; ++b) { burst(on); e.render(L.data(), R.data(), n, xL.data(), xR.data()); for (float x : L) s += double(x) * x; }
            return std::sqrt(s / (blocks * n));
        };
        rms(false, 40);
        const double quiet = rms(false, 20), open = rms(true, 20);
        check(open > 0.02 && quiet < open * 0.02, fmt("VCA ON/EXT: a held note sounds with the input (%.3f), not without it (%.5f)", open, quiet));
    }
    {   // the input into layer 1's filter, at its EXT level
        Engine e; e.prepare(48000, n);
        LayerParams p = plain(); p.osc1Level = 0.0f; p.extLevel = 0.6f; e.layer(0).setParams(p);
        e.noteOn(60, 1);
        double s = 0;
        for (int b = 0; b < 20; ++b) {
            for (int i = 0; i < n; ++i) xL[size_t(i)] = xR[size_t(i)] = 0.3f * std::sin(0.05f * float(b * n + i));
            e.render(L.data(), R.data(), n, xL.data(), xR.data());
            for (float x : L) s += double(x) * x;
        }
        check(std::sqrt(s / (20 * n)) > 0.02, fmt("EXT IN: the input through layer 1's filter and VCA (%.3f)", std::sqrt(s / (20 * n))));
    }
}

int main() {
    testPitch();
    testAliasing();
    testFilterCore();
    testSelfOsc();
    testEnvelopes();
    testVoices();
    testGlide();
    testDecimator();
    testDelay();
    testMatrix();
    testNoteRnd();
    testVary();
    testPlayModes();
    testArp();
    testExtIn();
    testStability();
    testCost();
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
