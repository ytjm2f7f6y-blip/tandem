// Tandem: the engine's tests. Each prints what it measured; a failed check makes the run fail.
//   cmake -B build && cmake --build build && ctest --test-dir build --output-on-failure
#include "td_engine.hpp"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>

using namespace td;

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
    testStability();
    testCost();
    std::printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
