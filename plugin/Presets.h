// Tandem: the factory presets. Each lists only what differs from the defaults, by parameter
// key and in the parameter's own units (a choice is its index, a switch 0 or 1).
#pragma once

namespace tpre {

struct Set { const char* key; float v; };
struct Preset { const char* name; Set s[40]; };

inline const Preset kFactory[] = {
    {"init", {{nullptr, 0}}},
    // Binaural: every voice is two ears, the sisters spread across them.
    {"twin saws", {{"osc2_level", 0.45f}, {"osc2_fine", 9}, {"detune", 0.55f}, {"cutoff", 0.6f}, {"resonance", 0.12f}, {"vcf_mod", 0.28f},
                   {"e1_attack", 0.5f}, {"e1_decay", 0.62f}, {"e1_sustain", 0.55f}, {"e1_release", 0.6f},
                   {"e2_attack", 0.48f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.9f}, {"e2_release", 0.58f}, {"voices", 2},
                   {"fx_send", 0.35f}, {"fx_balance", 0.75f}, {"rv_size", 0.62f}, {"drift", 0.25f}, {"vel_vca", 0.15f}, {nullptr, 0}}},
    {"sync lead", {{"osc1_level", 0.0f}, {"osc2_level", 0.6f}, {"sync", 1}, {"osc2_tune", 7}, {"mod_osc", 2}, {"pitch_src", 0}, {"pitch_mod", 0.42f},
                   {"e1_decay", 0.46f}, {"e1_sustain", 0.0f}, {"cutoff", 0.78f}, {"resonance", 0.18f}, {"voice_mode", 2}, {"glide", 0.28f},
                   {"fx_send", 0.25f}, {"fx_balance", 0.15f}, {"dly_time", 6}, {"dly_fb", 0.3f}, {"lfo2_pitch", 0.2f}, {nullptr, 0}}},
    {"resonant bass", {{"octave", 1}, {"osc2_wave", 2}, {"osc2_level", 0.45f}, {"osc1_level", 0.62f}, {"osc2_fine", -6}, {"res_type", 1},
                       {"cutoff", 0.27f}, {"resonance", 0.62f}, {"vcf_mod", 0.55f}, {"e1_decay", 0.37f}, {"e1_sustain", 0.08f}, {"e1_release", 0.2f},
                       {"e2_sustain", 0.85f}, {"e2_release", 0.15f}, {"voice_mode", 1}, {"keytrack", 1}, {"drv_mode", 1}, {"drv_amount", 0.35f},
                       {"vel_vcf", 0.4f}, {nullptr, 0}}},
    // Cross modulation: OSC 2 frequency-modulates OSC 1.
    {"glass bells", {{"osc1_wave", 0}, {"osc2_wave", 0}, {"osc2_tune", 7}, {"osc2_fine", 3}, {"osc2_level", 0.0f}, {"osc1_level", 0.6f},
                     {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.45f}, {"e2_decay", 0.7f}, {"e2_sustain", 0.0f}, {"e2_release", 0.64f},
                     {"cutoff", 0.86f}, {"route", 1}, {"voices", 2}, {"fx_send", 0.42f}, {"fx_balance", 0.8f}, {"rv_size", 0.7f},
                     {"m1_src", 4}, {"m1_dst", 7}, {"m1_amt", 0.35f}, {"e1_decay", 0.55f}, {nullptr, 0}}},
    // Two band passes side by side, pulled apart: a vowel. LFO 1 walks it.
    {"vowel pad", {{"osc2_wave", 3}, {"osc2_level", 0.5f}, {"wave_src", 2}, {"wave_mod", 0.5f}, {"mod_osc", 2}, {"route", 1}, {"type_par", 2},
                   {"delta", 0.42f}, {"cutoff", 0.5f}, {"resonance", 0.58f}, {"vcf_src", 1}, {"vcf_mod", 0.42f}, {"lfo1_rate", 0.22f}, {"lfo1_wave", 0},
                   {"e2_attack", 0.52f}, {"e2_release", 0.6f}, {"voices", 2}, {"ch_on", 1}, {"ch_mix", 0.45f}, {"fx_send", 0.3f}, {"fx_balance", 0.7f},
                   {"pan_spread", 0.7f}, {nullptr, 0}}},
    {"phase strings", {{"detune", 0.42f}, {"osc2_wave", 3}, {"osc2_level", 0.4f}, {"wave_src", 2}, {"wave_mod", 0.45f}, {"route", 2}, {"type_ser", 0},
                       {"cutoff", 0.58f}, {"resonance", 0.4f}, {"vcf_src", 1}, {"vcf_mod", 0.5f}, {"lfo1_rate", 0.18f}, {"lfo1_wave", 0},
                       {"e2_attack", 0.56f}, {"e2_release", 0.62f}, {"voices", 2}, {"fx_send", 0.32f}, {"fx_balance", 0.85f}, {"rv_size", 0.66f},
                       {"pan_spread", 0.8f}, {"drift", 0.3f}, {nullptr, 0}}},
    {"drone choir", {{"osc1_wave", 6}, {"osc1_alt1", 24}, {"osc2_wave", 11}, {"osc2_level", 0.32f}, {"detune", 0.3f}, {"mod_osc", 0}, {"type_st", 1},
                     {"cutoff", 0.56f}, {"e2_attack", 0.62f}, {"e2_release", 0.7f}, {"lfo1_wave", 7}, {"lfo1_rate", 0.3f}, {"wave_src", 2}, {"wave_mod", 0.35f},
                     {"fx_send", 0.45f}, {"fx_balance", 0.9f}, {"rv_size", 0.85f}, {"drift", 0.35f}, {nullptr, 0}}},
    // LFO 1 on the clock (an eighth), square, into the cutoff; unison.
    {"wobble unison", {{"voice_mode", 3}, {"unison", 1}, {"osc2_level", 0.5f}, {"osc2_wave", 2}, {"octave", 1}, {"cutoff", 0.34f}, {"resonance", 0.48f},
                       {"vcf_src", 1}, {"vcf_mod", 0.55f}, {"lfo1_wave", 3}, {"lfo1_sync", 1}, {"lfo1_rate", 0.7059f}, {"lfo1_mode", 2},
                       {"drv_mode", 2}, {"drv_amount", 0.4f}, {"e2_sustain", 1.0f}, {"e2_release", 0.12f}, {nullptr, 0}}},
    // LFO 1 at audio rate following the keys, into OSC 2's channel; OSC 2 also rattles the filter.
    {"hf grit", {{"osc1_wave", 2}, {"osc2_level", 0.42f}, {"osc2_wave", 0}, {"lfo1_wave", 11}, {"lfo1_mode", 2}, {"lfo1_rate", 0.6f}, {"vcf_src", 2},
                 {"vcf_mod", 0.32f}, {"cutoff", 0.52f}, {"resonance", 0.36f}, {"e2_decay", 0.45f}, {"e2_sustain", 0.6f}, {"fx_send", 0.2f}, {nullptr, 0}}},
    // The new matrix and variation: NOTE RND brightens every fourth note; the wheel brings in a vibrato (via);
    // DICE's roll 0417 at VARY 4 keeps a repeated phrase from sounding like a machine.
    {"dice keys", {{"osc2_level", 0.35f}, {"osc2_wave", 3}, {"osc2_fine", 5}, {"cutoff", 0.52f}, {"resonance", 0.16f}, {"vcf_mod", 0.38f},
                   {"e1_decay", 0.5f}, {"e1_sustain", 0.2f}, {"e2_decay", 0.58f}, {"e2_sustain", 0.55f}, {"e2_release", 0.45f}, {"vel_vca", 0.35f},
                   {"rnd_count", 4}, {"m1_src", 12}, {"m1_dst", 2}, {"m1_amt", 0.12f},
                   {"lfo2_trig", 2}, {"lfo2_pitch", 0.0f}, {"lfo2_rate", 0.72f}, {"m2_src", 3}, {"m2_dst", 16}, {"m2_amt", 0.03f}, {"m2_via", 11},
                   {"vary", 0.4f}, {"vary_seed", 417}, {"fx_send", 0.28f}, {"fx_balance", 0.55f}, {"drift", 0.22f}, {nullptr, 0}}},
};
constexpr int kNumFactory = int(sizeof(kFactory) / sizeof(kFactory[0]));

}  // namespace tpre
