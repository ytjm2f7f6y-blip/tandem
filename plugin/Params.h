// Tandem: the plugin's parameters, and how they become the engine's settings.
//
// Every parameter of a layer has the id "l<layer>_<key>"; stage 1 has layer 1 only. The
// table below is the single list: the host sees these, presets store them, the window
// and the display pages find them by key.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "td_engine.hpp"

#include <functional>
#include <vector>

namespace tp {

// ---- value texts
inline juce::String ten(float v, int) { return juce::String(v * 10.0f, 1); }
inline juce::String tenBi(float v, int) { return (v > 0.005f ? "+" : "") + juce::String(v * 10.0f, 1); }
inline juce::String pct(float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
inline juce::String secs(float s) {
    if (s < 0.001f) return juce::String(s * 1000.0f, 2) + " ms";
    if (s < 0.1f) return juce::String(s * 1000.0f, 1) + " ms";
    if (s < 1.0f) return juce::String(juce::roundToInt(s * 1000.0f)) + " ms";
    return juce::String(s, s < 10.0f ? 2 : 1) + " s";
}
inline juce::String attackT(float v, int) { return secs(td::Env::attackTime(v)); }
inline juce::String decayT(float v, int) { return secs(td::Env::decayTime(v)); }
inline juce::String holdT(float v, int) { return v <= 0.0f ? juce::String("off") : secs(td::Env::holdTime(v)); }
inline juce::String fadeT(float v, int) { return v <= 0.0f ? juce::String("none") : secs(5.0f * v * v * v); }
inline juce::String hz(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String(v, v < 10 ? 2 : (v < 100 ? 1 : 0)) + " Hz"; }
inline juce::String cutoffT(float v, int) { return hz(16.0f * std::pow(2.0f, v * 10.3f)); }
inline juce::String semis(float v, int) { const int s = juce::roundToInt(v); return (s > 0 ? "+" : "") + juce::String(s) + " st"; }
inline juce::String cents(float v, int) { const int s = juce::roundToInt(v); return (s > 0 ? "+" : "") + juce::String(s) + " ct"; }
inline juce::String db(float v, int) { return (v > 0.05f ? "+" : "") + juce::String(v, 1) + " dB"; }
inline juce::String glideT(float v, int) { return v <= 0.001f ? juce::String("off") : secs(10.0f * std::pow(v, 2.5f)) + " / oct"; }
inline juce::String panT(float v, int) {
    if (std::abs(v) < 0.02f) return "centre";
    return juce::String(juce::roundToInt(std::abs(v) * 100.0f)) + (v < 0 ? " % left" : " % right");
}

// ---- choices
inline juce::StringArray altNames() {
    juce::StringArray a;
    for (int i = 0; i < td::WaveBank::kNumAlt; ++i) a.add(juce::String(i + 1) + " " + td::WaveBank::altName(i));
    return a;
}
inline const juce::StringArray kOsc1Waves{"Sine", "Sawtooth", "Square", "Pulse", "Triangle", "Noise", "Alt 1", "Alt 2", "Alt 3", "Alt 4", "Alt 5", "Alt 6"};
inline const juce::StringArray kOsc2Waves{"Sine", "Sawtooth", "Square", "Pulse", "Triangle", "Noise", "Alt 1", "Alt 2", "Alt 3", "Drone Square", "Drone Saw", "Drone Sine"};
inline const juce::StringArray kSync{"Off", "2-1", "1-2"};
inline const juce::StringArray kModOsc{"OSC 1", "Both", "OSC 2"};
inline const juce::StringArray kWaveSrc{"Manual", "ENV 1", "LFO 1"};
inline const juce::StringArray kPitchSrc{"ENV 1", "LFO 1", "Cross"};
inline const juce::StringArray kKeytrack{"Off", "Half", "Full"};
inline const juce::StringArray kVcfSrc{"ENV 1", "LFO 1", "OSC 2"};
inline const juce::StringArray kRoute{"Stereo", "Parallel", "Series"};
inline const juce::StringArray kStereoTypes{"4PLP", "2PLP", "1PLP", "3PHP", "2PHP", "1PHP", "4PBP", "2PBP", "4PAP", "4PS", "1PLP+3PHP", "3PLP+1PHP"};
inline const juce::StringArray kParallelTypes{"2PLP+2PLP", "1PLP+1PLP", "2PBP+2PBP", "4PBP+4PBP"};
inline const juce::StringArray kSeriesTypes{"4PS>1PLP", "4PS>2PLP", "4PS>1PHP", "4PS>4PS", "1PLP>1PLP", "1PLP>1PHP"};
inline const juce::StringArray kResType{"Type 1", "Type 2"};
inline const juce::StringArray kLfo1Waves{"Sine", "Sawtooth", "Sample & Hold", "Square", "Ramp", "Noise", "Triangle", "Noisy Sine", "User 1", "User 2", "HF", "HF Track"};
inline const juce::StringArray kLfo1Modes{"Free", "Once", "Reset"};
inline const juce::StringArray kEnvModes{"Normal", "Inverted", "Loop"};
inline const juce::StringArray kVcaSrc{"ENV 2", "ENV 3", "On"};
inline const juce::StringArray kVoiceModes{"Poly 1", "Solo", "Legato", "Unison", "Poly 2"};
inline const juce::StringArray kUnison{"Level 1", "Level 2", "Octave", "Fifth", "Major"};
inline const juce::StringArray kVoices{"2", "4", "8", "16"};
constexpr int kVoiceCounts[4] = {2, 4, 8, 16};
inline const juce::StringArray kOctave{"-2", "-1", "0", "+1", "+2"};
inline const juce::StringArray kLfo2Waves{"Triangle", "Sawtooth", "Sample & Hold", "Square", "Ramp", "Noise"};
inline const juce::StringArray kLfo2Trig{"Wheel", "AT & Wheel", "On"};
inline const juce::StringArray kModSources{"Off", "OSC 2", "LFO 1", "LFO 2", "ENV 1", "ENV 2", "Velocity", "Aftertouch", "Expression", "Env Follower", "Note"};
inline const juce::StringArray kModDests{"Off", "LFO 1 Rate", "Cutoff", juce::String::fromUTF8("\xce\x94-Cutoff"), "ENV 1 > Cutoff", "ENV 1 Depth", "LFO 1 Depth",
                                         "OSC 2 > OSC 1", "OSC 1 Wave", "OSC 1 Mix", "OSC 2 Mix", "Resonance", "ENV 1 Decay", "Delay Time", "VCA Level", "ENV 1 Attack"};
inline const juce::StringArray kDriveModes{"Off", "Harmonic", "Overdrive", "Fuzz"};
inline const juce::StringArray kDelayDivs{"1/32", "1/32.", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T", "1/4", "1/4.", "1/2T", "1/2", "1/2.", "1/1T", "1/1"};
inline const juce::StringArray kDelayModes{"Stereo", "Ping-Pong", "Mono"};
inline const juce::StringArray kOnOff{"Off", "On"};

// ---- the table
enum class Kind { Float, Choice, Bool };
struct Def {
    juce::String key, name;
    Kind kind = Kind::Float;
    float lo = 0, hi = 1, def = 0, step = 0;
    juce::StringArray choices;
    juce::String (*fmt)(float, int) = nullptr;
};

inline std::vector<Def> layerDefs() {
    std::vector<Def> d;
    auto F = [&](const char* k, const char* n, float lo, float hi, float def, juce::String (*fm)(float, int) = ten, float step = 0) {
        d.push_back({k, n, Kind::Float, lo, hi, def, step, {}, fm});
    };
    auto C = [&](const char* k, const char* n, const juce::StringArray& ch, int def) { d.push_back({k, n, Kind::Choice, 0, float(ch.size() - 1), float(def), 1, ch, nullptr}); };
    auto B = [&](const char* k, const char* n, bool def) { d.push_back({k, n, Kind::Bool, 0, 1, def ? 1.0f : 0.0f, 1, {}, nullptr}); };
    const auto alts = altNames();

    // oscillators
    C("osc1_wave", "OSC 1 Wave", kOsc1Waves, td::kWSaw);
    C("osc2_wave", "OSC 2 Wave", kOsc2Waves, td::kWSaw);
    F("osc1_tune", "OSC 1 Tune", -7, 7, 0, semis, 1);
    F("osc1_fine", "OSC 1 Fine", -50, 50, 0, cents);
    F("osc2_tune", "OSC 2 Tune", -7, 7, 0, semis, 1);
    F("osc2_fine", "OSC 2 Fine", -50, 50, 0, cents);
    C("sync", "Sync", kSync, 0);
    C("octave", "Octave", kOctave, 2);
    C("mod_osc", "Mod Applies To", kModOsc, 1);
    F("detune", "Detune", 0, 1, 0);
    C("wave_src", "Wave/PWM Source", kWaveSrc, 0);
    F("wave_mod", "Wave/PWM", 0, 1, 0);
    C("pitch_src", "Pitch Mod Source", kPitchSrc, 0);
    F("pitch_mod", "Pitch Mod", 0, 1, 0);
    for (int i = 0; i < 6; ++i) C(("osc1_alt" + juce::String(i + 1)).toRawUTF8(), ("OSC 1 Alt Slot " + juce::String(i + 1)).toRawUTF8(), alts, i);
    for (int i = 0; i < 3; ++i) C(("osc2_alt" + juce::String(i + 1)).toRawUTF8(), ("OSC 2 Alt Slot " + juce::String(i + 1)).toRawUTF8(), alts, 6 + i);
    C("lfo_alt1", "LFO 1 User Wave 1", alts, 20);
    C("lfo_alt2", "LFO 1 User Wave 2", alts, 15);
    // mixer
    F("osc1_level", "OSC 1 Level", 0, 1, 0.55f);
    F("osc2_level", "OSC 2 Level", 0, 1, 0);
    F("ext_level", "EXT Level", 0, 1, 0);
    // filter
    F("cutoff", "Cutoff", 0, 1, 1.0f, cutoffT);
    F("resonance", "Resonance", 0, 1, 0);
    C("keytrack", "Keytrack", kKeytrack, 0);
    C("vcf_src", "VCF Mod Source", kVcfSrc, 0);
    F("vcf_mod", "VCF Mod", 0, 1, 0);
    F("delta", "Delta Cutoff", -1, 1, 0, tenBi);
    C("route", "Filter Routing", kRoute, 0);
    C("type_st", "Stereo Filter Type", kStereoTypes, 0);
    C("type_par", "Parallel Filter Type", kParallelTypes, 0);
    C("type_ser", "Series Filter Type", kSeriesTypes, 0);
    C("res_type", "Resonance Type", kResType, 0);
    // LFO 1
    C("lfo1_wave", "LFO 1 Wave", kLfo1Waves, td::kLTri);
    C("lfo1_mode", "LFO 1 Mode", kLfo1Modes, 0);
    F("lfo1_rate", "LFO 1 Rate", 0, 1, 0.5f);
    F("lfo1_delay", "LFO 1 Delay", 0, 1, 0, fadeT);
    F("lfo1_phase", "LFO 1 Phase", 0, 1, 0, [](float v, int) { return juce::String(juce::roundToInt(v * 360.0f)) + juce::String::fromUTF8("\xc2\xb0"); });
    B("lfo1_sync", "LFO 1 Clock Sync", false);
    // envelopes
    F("e1_hold", "ENV 1 Hold", 0, 1, 0, holdT);
    F("e1_attack", "ENV 1 Attack", 0, 1, 0, attackT);
    F("e1_decay", "ENV 1 Decay", 0, 1, 0.5f, decayT);
    F("e1_sustain", "ENV 1 Sustain", 0, 1, 0);
    F("e1_release", "ENV 1 Release", 0, 1, 0.3f, decayT);
    C("e1_mode", "ENV 1 Mode", kEnvModes, 0);
    F("e2_attack", "ENV 2 Attack", 0, 1, 0, attackT);
    F("e2_decay", "ENV 2 Decay", 0, 1, 0.4f, decayT);
    F("e2_sustain", "ENV 2 Sustain", 0, 1, 1.0f);
    F("e2_release", "ENV 2 Release", 0, 1, 0.25f, decayT);
    C("vca_src", "VCA Source", kVcaSrc, 0);
    C("e3_mode", "ENV 3 Mode", kEnvModes, 0);
    F("e3_hold", "ENV 3 Hold", 0, 1, 0, holdT);
    F("e3_attack", "ENV 3 Attack", 0, 1, 0.2f, attackT);
    F("e3_decay", "ENV 3 Decay", 0, 1, 0.5f, decayT);
    F("e3_sustain", "ENV 3 Sustain", 0, 1, 0.5f);
    F("e3_release", "ENV 3 Release", 0, 1, 0.4f, decayT);
    F("vel_vca", "Velocity to VCA", -1, 1, 0.25f, tenBi);
    F("vel_vcf", "Velocity to ENV 1", -1, 1, 0, tenBi);
    // voices
    C("voice_mode", "Voice Mode", kVoiceModes, 0);
    C("unison", "Unison", kUnison, 1);
    C("voices", "Voices", kVoices, 1);
    F("drift", "Drift", 0, 1, 0.15f);
    F("pan_spread", "Voice Pan Spread", 0, 1, 0);
    F("glide", "Portamento", 0, 1, 0, glideT);
    // performance
    F("bend_range", "Bend Range", 0, 12, 2, semis, 1);
    F("bend_vcf", "Bend to Cutoff", -1, 1, 0, tenBi);
    C("lfo2_wave", "LFO 2 Wave", kLfo2Waves, 0);
    C("lfo2_trig", "LFO 2 Trigger", kLfo2Trig, 0);
    F("lfo2_rate", "LFO 2 Rate", 0, 1, 0.45f, [](float v, int) { return hz(0.05f * std::pow(600.0f, v)); });
    F("lfo2_delay", "LFO 2 Delay", 0, 1, 0, fadeT);
    F("lfo2_ratemod", "LFO 2 Rate Mod", 0, 1, 0);
    F("lfo2_pitch", "LFO 2 to Pitch", 0, 1, 0.15f);
    F("lfo2_vcf", "LFO 2 to Cutoff", 0, 1, 0);
    F("lfo2_vca", "LFO 2 to VCA", 0, 1, 0);
    // matrix
    for (int i = 0; i < td::kMatrixSlots; ++i) {
        const juce::String n(i + 1);
        C(("m" + n + "_src").toRawUTF8(), ("Matrix " + n + " Source").toRawUTF8(), kModSources, 0);
        C(("m" + n + "_dst").toRawUTF8(), ("Matrix " + n + " Destination").toRawUTF8(), kModDests, 0);
        F(("m" + n + "_amt").toRawUTF8(), ("Matrix " + n + " Amount").toRawUTF8(), -1, 1, 0, [](float v, int) { const int a = juce::roundToInt(v * 63.0f); return (a > 0 ? "+" : "") + juce::String(a); });
    }
    // insert effects
    C("drv_mode", "Drive", kDriveModes, 0);
    F("drv_tone", "Drive Tone", 0, 1, 0.5f);
    F("drv_amount", "Drive Amount", 0, 1, 0.3f);
    F("drv_mix", "Drive Mix", 0, 1, 1.0f, pct);
    B("ch_on", "Chorus", false);
    F("ch_rate", "Chorus Rate", 0, 1, 0.3f, [](float v, int) { return hz(0.05f * std::pow(100.0f, v)); });
    F("ch_depth", "Chorus Depth", 0, 1, 0.4f);
    F("ch_delay", "Chorus Delay", 0, 1, 0.3f, [](float v, int) { return juce::String(1.5f + 18.0f * v, 1) + " ms"; });
    F("ch_mix", "Chorus Mix", 0, 1, 0.5f, pct);
    F("eq_low", "EQ Low", -15, 15, 0, db);
    F("eq_mid", "EQ Mid", -15, 15, 0, db);
    F("eq_high", "EQ High", -15, 15, 0, db);
    F("eq_q", "EQ Mid Width", 0.3f, 4.0f, 0.7f, [](float v, int) { return "Q " + juce::String(v, 2); });
    // send effects
    C("dly_time", "Delay Time", kDelayDivs, 7);
    C("dly_mode", "Delay Mode", kDelayModes, 0);
    F("dly_fb", "Delay Feedback", 0, 1, 0.35f);
    F("dly_damp", "Delay Damping", 0, 1, 0.3f);
    F("fx_balance", "Delay/Reverb Balance", 0, 1, 0.5f, [](float v, int) { const int r = juce::roundToInt(v * 127.0f); return juce::String(127 - r) + "/" + juce::String(r); });
    B("freeze", "Delay Freeze", false);
    F("rv_size", "Reverb Size", 0, 1, 0.5f);
    F("rv_damp", "Reverb Damping", 0, 1, 0.4f);
    F("rv_lowcut", "Reverb Low Cut", 0, 1, 0.2f, [](float v, int) { return hz(20.0f * std::pow(40.0f, v)); });
    F("rv_predelay", "Reverb Pre-delay", 0, 1, 0.1f, [](float v, int) { return juce::String(juce::roundToInt(200.0f * v * v)) + " ms"; });
    F("rv_width", "Reverb Width", 0, 1, 0.8f, pct);
    // mixer
    F("level", "Level", 0, 1, 0.75f);
    F("pan", "Pan", -1, 1, 0, panT);
    F("fx_send", "FX Send", 0, 1, 0);
    return d;
}

inline juce::String layerId(int layer, const juce::String& key) { return "l" + juce::String(layer + 1) + "_" + key; }
inline const char* kVolumeId = "volume";
constexpr int kLayersExposed = 1;     // stage 1

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto defs = layerDefs();
    for (int L = 0; L < kLayersExposed; ++L) {
        const String pre = kLayersExposed > 1 ? "L" + String(L + 1) + " " : String();
        for (const auto& d : defs) {
            const ParameterID id{layerId(L, d.key), 1};
            if (d.kind == Kind::Choice) layout.add(std::make_unique<AudioParameterChoice>(id, pre + d.name, d.choices, int(d.def)));
            else if (d.kind == Kind::Bool) layout.add(std::make_unique<AudioParameterBool>(id, pre + d.name, d.def >= 0.5f));
            else {
                auto a = AudioParameterFloatAttributes();
                if (d.fmt) a = a.withStringFromValueFunction(d.fmt);
                NormalisableRange<float> r(d.lo, d.hi, d.step);
                layout.add(std::make_unique<AudioParameterFloat>(id, pre + d.name, r, d.def, a));
            }
        }
    }
    auto a = AudioParameterFloatAttributes().withStringFromValueFunction(ten);
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{kVolumeId, 1}, "Volume", NormalisableRange<float>(0, 1), 0.8f, a));
    return layout;
}

// Reads a layer's parameters (raw values, by key) into the engine's settings.
struct LayerReader {
    std::vector<std::atomic<float>*> raw;    // in layerDefs() order
    std::vector<juce::String> keys;
    void attach(juce::AudioProcessorValueTreeState& s, int layer) {
        raw.clear(); keys.clear();
        for (const auto& d : layerDefs()) { raw.push_back(s.getRawParameterValue(layerId(layer, d.key))); keys.push_back(d.key); jassert(raw.back() != nullptr); }
    }
    float v(int i) const { return raw[size_t(i)]->load(std::memory_order_relaxed); }
    int idx(const char* key) const { for (size_t i = 0; i < keys.size(); ++i) if (keys[i] == key) return int(i); jassertfalse; return 0; }

    // Indexes, worked out once.
    struct Ix { int osc1Wave, osc2Wave, t1, f1, t2, f2, sync, octave, modOsc, detune, waveSrc, waveMod, pitchSrc, pitchMod, alt1[6], alt2[3], altL[2],
                l1, l2, lx, cutoff, res, kt, vsrc, vmod, delta, route, tSt, tPar, tSer, resType, lw, lm, lr, ld, lph, lsy,
                e1h, e1a, e1d, e1s, e1r, e1m, e2a, e2d, e2s, e2r, vca, e3m, e3h, e3a, e3d, e3s, e3r, velVca, velVcf,
                vmode, uni, voices, drift, spread, glide, bendR, bendV, l2w, l2t, l2r, l2d, l2rm, l2p, l2v, l2a, mS[7], mD[7], mA[7],
                drvM, drvT, drvA, drvX, chOn, chR, chD, chDl, chX, eqL, eqM, eqH, eqQ, dT, dM, dF, dD, bal, frz, rS, rD, rL, rP, rW, level, pan, send; } ix{};
    void index() {
        auto I = [&](const char* k) { return idx(k); };
        ix.osc1Wave = I("osc1_wave"); ix.osc2Wave = I("osc2_wave"); ix.t1 = I("osc1_tune"); ix.f1 = I("osc1_fine"); ix.t2 = I("osc2_tune"); ix.f2 = I("osc2_fine");
        ix.sync = I("sync"); ix.octave = I("octave"); ix.modOsc = I("mod_osc"); ix.detune = I("detune"); ix.waveSrc = I("wave_src"); ix.waveMod = I("wave_mod");
        ix.pitchSrc = I("pitch_src"); ix.pitchMod = I("pitch_mod");
        for (int i = 0; i < 6; ++i) ix.alt1[i] = idx(("osc1_alt" + juce::String(i + 1)).toRawUTF8());
        for (int i = 0; i < 3; ++i) ix.alt2[i] = idx(("osc2_alt" + juce::String(i + 1)).toRawUTF8());
        ix.altL[0] = I("lfo_alt1"); ix.altL[1] = I("lfo_alt2");
        ix.l1 = I("osc1_level"); ix.l2 = I("osc2_level"); ix.lx = I("ext_level");
        ix.cutoff = I("cutoff"); ix.res = I("resonance"); ix.kt = I("keytrack"); ix.vsrc = I("vcf_src"); ix.vmod = I("vcf_mod"); ix.delta = I("delta");
        ix.route = I("route"); ix.tSt = I("type_st"); ix.tPar = I("type_par"); ix.tSer = I("type_ser"); ix.resType = I("res_type");
        ix.lw = I("lfo1_wave"); ix.lm = I("lfo1_mode"); ix.lr = I("lfo1_rate"); ix.ld = I("lfo1_delay"); ix.lph = I("lfo1_phase"); ix.lsy = I("lfo1_sync");
        ix.e1h = I("e1_hold"); ix.e1a = I("e1_attack"); ix.e1d = I("e1_decay"); ix.e1s = I("e1_sustain"); ix.e1r = I("e1_release"); ix.e1m = I("e1_mode");
        ix.e2a = I("e2_attack"); ix.e2d = I("e2_decay"); ix.e2s = I("e2_sustain"); ix.e2r = I("e2_release"); ix.vca = I("vca_src");
        ix.e3m = I("e3_mode"); ix.e3h = I("e3_hold"); ix.e3a = I("e3_attack"); ix.e3d = I("e3_decay"); ix.e3s = I("e3_sustain"); ix.e3r = I("e3_release");
        ix.velVca = I("vel_vca"); ix.velVcf = I("vel_vcf");
        ix.vmode = I("voice_mode"); ix.uni = I("unison"); ix.voices = I("voices"); ix.drift = I("drift"); ix.spread = I("pan_spread"); ix.glide = I("glide");
        ix.bendR = I("bend_range"); ix.bendV = I("bend_vcf");
        ix.l2w = I("lfo2_wave"); ix.l2t = I("lfo2_trig"); ix.l2r = I("lfo2_rate"); ix.l2d = I("lfo2_delay"); ix.l2rm = I("lfo2_ratemod"); ix.l2p = I("lfo2_pitch");
        ix.l2v = I("lfo2_vcf"); ix.l2a = I("lfo2_vca");
        for (int i = 0; i < 7; ++i) {
            const juce::String n(i + 1);
            ix.mS[i] = idx(("m" + n + "_src").toRawUTF8()); ix.mD[i] = idx(("m" + n + "_dst").toRawUTF8()); ix.mA[i] = idx(("m" + n + "_amt").toRawUTF8());
        }
        ix.drvM = I("drv_mode"); ix.drvT = I("drv_tone"); ix.drvA = I("drv_amount"); ix.drvX = I("drv_mix");
        ix.chOn = I("ch_on"); ix.chR = I("ch_rate"); ix.chD = I("ch_depth"); ix.chDl = I("ch_delay"); ix.chX = I("ch_mix");
        ix.eqL = I("eq_low"); ix.eqM = I("eq_mid"); ix.eqH = I("eq_high"); ix.eqQ = I("eq_q");
        ix.dT = I("dly_time"); ix.dM = I("dly_mode"); ix.dF = I("dly_fb"); ix.dD = I("dly_damp"); ix.bal = I("fx_balance"); ix.frz = I("freeze");
        ix.rS = I("rv_size"); ix.rD = I("rv_damp"); ix.rL = I("rv_lowcut"); ix.rP = I("rv_predelay"); ix.rW = I("rv_width");
        ix.level = I("level"); ix.pan = I("pan"); ix.send = I("fx_send");
    }

    td::LayerParams read() const {
        td::LayerParams p;
        auto n = [&](int i) { return juce::roundToInt(v(i)); };
        p.osc1Wave = n(ix.osc1Wave); p.osc2Wave = n(ix.osc2Wave);
        p.osc1Tune = float(n(ix.t1)); p.osc1Fine = v(ix.f1); p.osc2Tune = float(n(ix.t2)); p.osc2Fine = v(ix.f2);
        p.sync = n(ix.sync); p.octave = n(ix.octave) - 2; p.modOsc = n(ix.modOsc); p.detune = v(ix.detune);
        p.waveSrc = n(ix.waveSrc); p.waveMod = v(ix.waveMod); p.pitchSrc = n(ix.pitchSrc); p.pitchMod = v(ix.pitchMod);
        for (int i = 0; i < 6; ++i) p.osc1Alt[i] = n(ix.alt1[i]);
        for (int i = 0; i < 3; ++i) p.osc2Alt[i] = n(ix.alt2[i]);
        p.lfoAlt[0] = n(ix.altL[0]); p.lfoAlt[1] = n(ix.altL[1]);
        p.osc1Level = v(ix.l1); p.osc2Level = v(ix.l2); p.extLevel = v(ix.lx);
        p.cutoff = v(ix.cutoff); p.resonance = v(ix.res); p.keytrack = n(ix.kt); p.vcfSrc = n(ix.vsrc); p.vcfMod = v(ix.vmod); p.deltaCutoff = v(ix.delta);
        p.route = n(ix.route); p.stereoType = n(ix.tSt); p.parallelType = n(ix.tPar); p.seriesType = n(ix.tSer); p.resType = n(ix.resType);
        p.lfo1Wave = n(ix.lw); p.lfo1Mode = n(ix.lm); p.lfo1Rate = v(ix.lr); p.lfo1Delay = v(ix.ld); p.lfo1Phase = v(ix.lph); p.lfo1Sync = v(ix.lsy) >= 0.5f;
        p.e1Hold = v(ix.e1h); p.e1A = v(ix.e1a); p.e1D = v(ix.e1d); p.e1S = v(ix.e1s); p.e1R = v(ix.e1r); p.e1Mode = n(ix.e1m);
        p.e2A = v(ix.e2a); p.e2D = v(ix.e2d); p.e2S = v(ix.e2s); p.e2R = v(ix.e2r); p.vcaSrc = n(ix.vca);
        p.e3Mode = n(ix.e3m); p.e3Hold = v(ix.e3h); p.e3A = v(ix.e3a); p.e3D = v(ix.e3d); p.e3S = v(ix.e3s); p.e3R = v(ix.e3r);
        p.velVca = v(ix.velVca); p.velVcf = v(ix.velVcf);
        p.voiceMode = n(ix.vmode); p.unisonMode = n(ix.uni); p.voices = kVoiceCounts[juce::jlimit(0, 3, n(ix.voices))];
        p.drift = v(ix.drift); p.panSpread = v(ix.spread); p.portamento = v(ix.glide);
        p.bendRange = float(n(ix.bendR)); p.bendVcf = v(ix.bendV);
        p.lfo2Wave = n(ix.l2w); p.lfo2Trig = n(ix.l2t); p.lfo2Rate = v(ix.l2r); p.lfo2Delay = v(ix.l2d); p.lfo2RateMod = v(ix.l2rm);
        p.lfo2Pitch = v(ix.l2p); p.lfo2Vcf = v(ix.l2v); p.lfo2Vca = v(ix.l2a);
        for (int i = 0; i < 7; ++i) { p.mSrc[i] = n(ix.mS[i]); p.mDst[i] = n(ix.mD[i]); p.mAmt[i] = v(ix.mA[i]); }
        p.drvMode = n(ix.drvM); p.drvTone = v(ix.drvT); p.drvAmt = v(ix.drvA); p.drvMix = v(ix.drvX);
        p.chOn = v(ix.chOn) >= 0.5f; p.chRate = v(ix.chR); p.chDepth = v(ix.chD); p.chDelay = v(ix.chDl); p.chMix = v(ix.chX);
        p.eqLow = v(ix.eqL); p.eqMid = v(ix.eqM); p.eqHigh = v(ix.eqH); p.eqQ = v(ix.eqQ);
        p.dlyDiv = n(ix.dT); p.dlyMode = n(ix.dM); p.dlyFb = v(ix.dF); p.dlyDamp = v(ix.dD); p.fxBal = v(ix.bal); p.freeze = v(ix.frz) >= 0.5f;
        p.rvSize = v(ix.rS); p.rvDamp = v(ix.rD); p.rvLowCut = v(ix.rL); p.rvPreDly = v(ix.rP); p.rvWidth = v(ix.rW);
        p.level = v(ix.level); p.pan = v(ix.pan); p.fxSend = v(ix.send);
        return p;
    }
};

}  // namespace tp
