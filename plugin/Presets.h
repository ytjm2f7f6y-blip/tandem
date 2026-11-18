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
    // ---- More of the factory sounds: basses, leads, pads, keys, plucks and patterns, effects.
    {"round bass", {{"level", 1.0f}, {"octave", 1}, {"osc1_level", 0.6f}, {"osc2_wave", 2}, {"osc2_level", 0.3f}, {"osc2_fine", -5}, {"cutoff", 0.36f}, {"resonance", 0.2f}, {"vcf_mod", 0.42f}, {"e1_decay", 0.42f}, {"e1_sustain", 0.15f}, {"e1_release", 0.25f}, {"e2_decay", 0.5f}, {"e2_sustain", 0.9f}, {"e2_release", 0.18f}, {"voice_mode", 2}, {"glide", 0.12f}, {"keytrack", 1}, {"route", 1}, {"vel_vcf", 0.3f}, {nullptr, 0}}},
    {"sub thump", {{"octave", 1}, {"osc1_wave", 0}, {"osc1_level", 0.75f}, {"osc2_wave", 4}, {"osc2_level", 0.25f}, {"pitch_src", 0}, {"pitch_mod", 0.25f}, {"mod_osc", 1}, {"e1_decay", 0.3f}, {"cutoff", 0.55f}, {"e2_decay", 0.55f}, {"e2_sustain", 0.6f}, {"e2_release", 0.2f}, {"voice_mode", 1}, {"route", 1}, {"drift", 0.05f}, {nullptr, 0}}},
    {"acid squelch", {{"octave", 1}, {"osc1_level", 0.6f}, {"cutoff", 0.3f}, {"resonance", 0.72f}, {"res_type", 1}, {"vcf_mod", 0.55f}, {"e1_decay", 0.38f}, {"e1_release", 0.2f}, {"e2_release", 0.12f}, {"voice_mode", 2}, {"glide", 0.15f}, {"drv_mode", 2}, {"drv_amount", 0.3f}, {"vel_vcf", 0.45f}, {"route", 1}, {"type_par", 0}, {nullptr, 0}}},
    {"reese", {{"octave", 1}, {"osc1_level", 0.55f}, {"osc2_level", 0.55f}, {"osc2_fine", 14}, {"detune", 0.25f}, {"cutoff", 0.42f}, {"resonance", 0.15f}, {"vcf_mod", 0.15f}, {"e2_release", 0.2f}, {"voice_mode", 2}, {"lfo1_wave", 6}, {"lfo1_rate", 0.3f}, {"m1_src", 2}, {"m1_dst", 2}, {"m1_amt", 0.08f}, {"drv_mode", 1}, {"drv_amount", 0.3f}, {nullptr, 0}}},
    {"pluck bass", {{"octave", 1}, {"osc1_wave", 3}, {"osc1_level", 0.6f}, {"osc2_level", 0.35f}, {"osc2_fine", 7}, {"cutoff", 0.33f}, {"resonance", 0.3f}, {"vcf_mod", 0.48f}, {"e1_decay", 0.4f}, {"e2_decay", 0.55f}, {"e2_sustain", 0.3f}, {"e2_release", 0.18f}, {"vel_vcf", 0.35f}, {"route", 1}, {nullptr, 0}}},
    {"fm bass", {{"octave", 1}, {"osc1_wave", 0}, {"osc1_level", 0.7f}, {"osc2_wave", 0}, {"osc2_level", 0.0f}, {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.35f}, {"m1_src", 4}, {"m1_dst", 7}, {"m1_amt", 0.4f}, {"e1_decay", 0.45f}, {"e1_sustain", 0.1f}, {"cutoff", 0.75f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.7f}, {"e2_release", 0.2f}, {"voice_mode", 1}, {"route", 1}, {nullptr, 0}}},
    {"drone bass", {{"level", 1.0f}, {"octave", 1}, {"osc1_level", 0.5f}, {"osc2_wave", 10}, {"osc2_level", 0.3f}, {"cutoff", 0.4f}, {"resonance", 0.35f}, {"lfo1_wave", 6}, {"lfo1_rate", 0.25f}, {"vcf_src", 1}, {"vcf_mod", 0.3f}, {"e2_attack", 0.4f}, {"e2_release", 0.5f}, {"fx_send", 0.2f}, {"fx_balance", 0.8f}, {nullptr, 0}}},
    {"brass lead", {{"osc1_level", 0.55f}, {"osc2_level", 0.5f}, {"osc2_fine", 8}, {"cutoff", 0.5f}, {"resonance", 0.12f}, {"vcf_mod", 0.4f}, {"e1_attack", 0.45f}, {"e1_decay", 0.55f}, {"e1_sustain", 0.6f}, {"e1_release", 0.4f}, {"e2_attack", 0.35f}, {"e2_sustain", 0.85f}, {"e2_release", 0.35f}, {"vel_vcf", 0.3f}, {"voice_mode", 2}, {"glide", 0.08f}, {"lfo2_pitch", 0.2f}, {"fx_send", 0.2f}, {"fx_balance", 0.6f}, {nullptr, 0}}},
    {"square lead", {{"osc1_wave", 2}, {"osc1_level", 0.6f}, {"osc2_wave", 2}, {"osc2_level", 0.35f}, {"osc2_fine", -10}, {"wave_src", 2}, {"wave_mod", 0.3f}, {"lfo1_rate", 0.45f}, {"cutoff", 0.72f}, {"resonance", 0.1f}, {"voice_mode", 1}, {"glide", 0.18f}, {"lfo2_trig", 2}, {"lfo2_delay", 0.5f}, {"lfo2_pitch", 0.18f}, {"lfo2_rate", 0.7f}, {"dly_time", 9}, {"fx_send", 0.25f}, {"fx_balance", 0.2f}, {nullptr, 0}}},
    {"screamer", {{"osc1_level", 0.55f}, {"osc2_level", 0.5f}, {"osc2_tune", 7}, {"cutoff", 0.6f}, {"resonance", 0.45f}, {"vcf_mod", 0.3f}, {"drv_mode", 3}, {"drv_amount", 0.45f}, {"drv_tone", 0.6f}, {"voice_mode", 2}, {"glide", 0.2f}, {"e2_release", 0.3f}, {"m1_src", 7}, {"m1_dst", 2}, {"m1_amt", 0.3f}, {"fx_send", 0.22f}, {"fx_balance", 0.3f}, {nullptr, 0}}},
    {"whistle", {{"osc1_wave", 0}, {"osc1_level", 0.7f}, {"osc2_wave", 4}, {"osc2_level", 0.2f}, {"osc2_fine", 3}, {"cutoff", 0.9f}, {"e2_attack", 0.45f}, {"e2_release", 0.45f}, {"lfo2_trig", 2}, {"lfo2_delay", 0.45f}, {"lfo2_pitch", 0.22f}, {"lfo2_rate", 0.7f}, {"voice_mode", 1}, {"glide", 0.22f}, {"fx_send", 0.35f}, {"fx_balance", 0.85f}, {nullptr, 0}}},
    {"sync sweep", {{"osc1_level", 0.0f}, {"osc2_level", 0.6f}, {"sync", 1}, {"mod_osc", 2}, {"pitch_src", 1}, {"pitch_mod", 0.35f}, {"lfo1_wave", 6}, {"lfo1_rate", 0.35f}, {"cutoff", 0.8f}, {"resonance", 0.15f}, {"voice_mode", 2}, {"e2_release", 0.3f}, {"fx_send", 0.2f}, {"fx_balance", 0.4f}, {nullptr, 0}}},
    {"metal lead", {{"osc1_wave", 2}, {"osc1_level", 0.6f}, {"osc2_wave", 0}, {"osc2_level", 0.0f}, {"osc2_tune", 5}, {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.3f}, {"cutoff", 0.75f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.7f}, {"voice_mode", 1}, {"dly_time", 6}, {"fx_send", 0.3f}, {"fx_balance", 0.35f}, {nullptr, 0}}},
    {"soft mono", {{"osc1_wave", 4}, {"osc1_level", 0.65f}, {"osc2_level", 0.2f}, {"osc2_fine", 5}, {"cutoff", 0.55f}, {"vcf_mod", 0.2f}, {"keytrack", 2}, {"e2_attack", 0.3f}, {"voice_mode", 2}, {"glide", 0.3f}, {"lfo2_pitch", 0.12f}, {"fx_send", 0.25f}, {"fx_balance", 0.7f}, {nullptr, 0}}},
    {"warm pad", {{"osc1_level", 0.5f}, {"osc2_level", 0.45f}, {"osc2_fine", 7}, {"detune", 0.35f}, {"cutoff", 0.48f}, {"resonance", 0.1f}, {"vcf_mod", 0.2f}, {"e1_attack", 0.7f}, {"e1_decay", 0.7f}, {"e1_sustain", 0.6f}, {"e1_release", 0.7f}, {"e2_attack", 0.68f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.9f}, {"e2_release", 0.7f}, {"ch_on", 1}, {"fx_send", 0.35f}, {"fx_balance", 0.8f}, {"rv_size", 0.7f}, {"drift", 0.25f}, {nullptr, 0}}},
    {"glass pad", {{"level", 1.0f}, {"osc1_wave", 4}, {"osc1_level", 0.5f}, {"osc2_wave", 2}, {"osc2_level", 0.3f}, {"osc2_tune", 7}, {"osc2_fine", 4}, {"route", 1}, {"type_par", 2}, {"delta", 0.3f}, {"cutoff", 0.68f}, {"resonance", 0.35f}, {"lfo1_wave", 6}, {"lfo1_rate", 0.25f}, {"vcf_src", 1}, {"vcf_mod", 0.2f}, {"e2_attack", 0.6f}, {"e2_release", 0.65f}, {"fx_send", 0.45f}, {"fx_balance", 0.85f}, {"rv_size", 0.8f}, {nullptr, 0}}},
    {"dark strings", {{"osc1_level", 0.5f}, {"osc2_level", 0.45f}, {"osc2_fine", -6}, {"detune", 0.45f}, {"cutoff", 0.5f}, {"resonance", 0.05f}, {"vcf_mod", 0.15f}, {"e2_attack", 0.6f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.85f}, {"e2_release", 0.6f}, {"ch_on", 1}, {"ch_depth", 0.5f}, {"eq_high", -3}, {"fx_send", 0.3f}, {"fx_balance", 0.75f}, {nullptr, 0}}},
    {"pwm ensemble", {{"osc1_wave", 3}, {"osc1_level", 0.5f}, {"osc2_wave", 3}, {"osc2_level", 0.45f}, {"osc2_fine", 9}, {"wave_src", 2}, {"wave_mod", 0.45f}, {"lfo1_rate", 0.42f}, {"mod_osc", 1}, {"cutoff", 0.62f}, {"e2_attack", 0.55f}, {"e2_sustain", 0.9f}, {"e2_release", 0.55f}, {"ch_on", 1}, {"fx_send", 0.3f}, {"fx_balance", 0.7f}, {nullptr, 0}}},
    {"slow swell", {{"osc1_level", 0.5f}, {"osc2_wave", 4}, {"osc2_level", 0.4f}, {"osc2_fine", 6}, {"cutoff", 0.35f}, {"resonance", 0.2f}, {"vcf_mod", 0.35f}, {"e1_attack", 0.8f}, {"e1_decay", 0.75f}, {"e1_sustain", 0.5f}, {"e1_release", 0.75f}, {"e2_attack", 0.75f}, {"e2_release", 0.75f}, {"fx_send", 0.5f}, {"fx_balance", 0.9f}, {"rv_size", 0.85f}, {nullptr, 0}}},
    {"air pad", {{"osc1_level", 0.45f}, {"osc2_wave", 5}, {"osc2_level", 0.2f}, {"type_st", 6}, {"cutoff", 0.66f}, {"resonance", 0.3f}, {"lfo1_wave", 0}, {"lfo1_rate", 0.2f}, {"vcf_src", 1}, {"vcf_mod", 0.25f}, {"e2_attack", 0.65f}, {"e2_release", 0.7f}, {"fx_send", 0.5f}, {"fx_balance", 0.9f}, {nullptr, 0}}},
    {"evolving", {{"osc1_level", 0.45f}, {"osc2_wave", 2}, {"osc2_level", 0.4f}, {"osc2_tune", 7}, {"wave_src", 2}, {"wave_mod", 0.4f}, {"lfo1_wave", 7}, {"lfo1_rate", 0.2f}, {"rnd_count", 0}, {"m1_src", 12}, {"m1_dst", 2}, {"m1_amt", 0.15f}, {"lfo2_trig", 2}, {"lfo2_rate", 0.3f}, {"lfo2_pitch", 0.0f}, {"m2_src", 3}, {"m2_dst", 17}, {"m2_amt", 0.5f}, {"cutoff", 0.5f}, {"e2_attack", 0.6f}, {"e2_release", 0.7f}, {"fx_send", 0.45f}, {"fx_balance", 0.85f}, {nullptr, 0}}},
    {"electric keys", {{"osc1_wave", 0}, {"osc1_level", 0.6f}, {"osc2_wave", 0}, {"osc2_level", 0.0f}, {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.18f}, {"m1_src", 4}, {"m1_dst", 7}, {"m1_amt", 0.3f}, {"m2_src", 6}, {"m2_dst", 7}, {"m2_amt", 0.25f}, {"e1_decay", 0.5f}, {"e2_decay", 0.72f}, {"e2_sustain", 0.0f}, {"e2_release", 0.45f}, {"vel_vca", 0.5f}, {"ch_on", 1}, {"ch_mix", 0.3f}, {"cutoff", 0.85f}, {"route", 1}, {nullptr, 0}}},
    {"clav", {{"osc1_wave", 3}, {"osc1_level", 0.6f}, {"cutoff", 0.55f}, {"resonance", 0.35f}, {"vcf_mod", 0.45f}, {"e1_decay", 0.35f}, {"e2_decay", 0.45f}, {"e2_sustain", 0.0f}, {"e2_release", 0.15f}, {"type_st", 7}, {"keytrack", 2}, {"vel_vcf", 0.5f}, {"drv_mode", 1}, {"drv_amount", 0.2f}, {nullptr, 0}}},
    {"organ", {{"osc1_wave", 0}, {"osc1_level", 0.5f}, {"osc2_wave", 0}, {"osc2_level", 0.35f}, {"osc2_tune", 7}, {"e2_attack", 0.1f}, {"e2_release", 0.15f}, {"vel_vca", 0.0f}, {"ch_on", 1}, {"ch_rate", 0.55f}, {"ch_depth", 0.5f}, {"cutoff", 0.8f}, {"drv_mode", 2}, {"drv_amount", 0.15f}, {"route", 1}, {nullptr, 0}}},
    {"music box", {{"octave", 3}, {"osc1_wave", 0}, {"osc1_level", 0.6f}, {"osc2_wave", 0}, {"osc2_level", 0.0f}, {"osc2_tune", 7}, {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.2f}, {"e2_decay", 0.65f}, {"e2_sustain", 0.0f}, {"e2_release", 0.6f}, {"fx_send", 0.35f}, {"fx_balance", 0.8f}, {"route", 1}, {nullptr, 0}}},
    {"poly brass", {{"osc1_level", 0.55f}, {"osc2_level", 0.5f}, {"osc2_fine", 6}, {"cutoff", 0.42f}, {"vcf_mod", 0.42f}, {"e1_attack", 0.42f}, {"e1_decay", 0.55f}, {"e1_sustain", 0.45f}, {"e1_release", 0.4f}, {"e2_attack", 0.3f}, {"e2_sustain", 0.9f}, {"e2_release", 0.35f}, {"vel_vcf", 0.3f}, {"route", 1}, {"type_par", 0}, {"fx_send", 0.15f}, {"fx_balance", 0.7f}, {nullptr, 0}}},
    {"house stab", {{"osc1_level", 0.55f}, {"osc2_wave", 2}, {"osc2_level", 0.45f}, {"osc2_fine", -8}, {"cutoff", 0.45f}, {"resonance", 0.25f}, {"vcf_mod", 0.4f}, {"e1_decay", 0.4f}, {"e2_decay", 0.48f}, {"e2_sustain", 0.0f}, {"e2_release", 0.25f}, {"dly_time", 7}, {"dly_fb", 0.3f}, {"fx_send", 0.25f}, {"fx_balance", 0.3f}, {nullptr, 0}}},
    {"pluck", {{"osc1_level", 0.55f}, {"osc2_wave", 2}, {"osc2_level", 0.3f}, {"osc2_fine", 5}, {"cutoff", 0.35f}, {"resonance", 0.2f}, {"vcf_mod", 0.5f}, {"e1_decay", 0.4f}, {"e1_release", 0.4f}, {"e2_decay", 0.55f}, {"e2_sustain", 0.0f}, {"e2_release", 0.45f}, {"vel_vcf", 0.35f}, {"dly_time", 6}, {"dly_fb", 0.35f}, {"fx_send", 0.3f}, {"fx_balance", 0.35f}, {nullptr, 0}}},
    {"harp", {{"osc1_wave", 4}, {"osc1_level", 0.6f}, {"osc2_level", 0.2f}, {"osc2_fine", 3}, {"cutoff", 0.55f}, {"vcf_mod", 0.35f}, {"e1_decay", 0.45f}, {"e2_decay", 0.68f}, {"e2_sustain", 0.0f}, {"e2_release", 0.6f}, {"fx_send", 0.35f}, {"fx_balance", 0.85f}, {"keytrack", 2}, {nullptr, 0}}},
    {"kalimba", {{"osc1_wave", 0}, {"osc1_level", 0.65f}, {"osc2_wave", 0}, {"osc2_level", 0.0f}, {"osc2_tune", 5}, {"pitch_src", 2}, {"mod_osc", 0}, {"pitch_mod", 0.25f}, {"m1_src", 4}, {"m1_dst", 7}, {"m1_amt", 0.4f}, {"e1_decay", 0.35f}, {"cutoff", 0.8f}, {"e2_decay", 0.6f}, {"e2_sustain", 0.0f}, {"e2_release", 0.5f}, {"route", 1}, {"fx_send", 0.25f}, {"fx_balance", 0.8f}, {nullptr, 0}}},
    {"dice arp", {{"osc1_level", 0.55f}, {"osc2_wave", 3}, {"osc2_level", 0.3f}, {"osc2_fine", 6}, {"cutoff", 0.4f}, {"resonance", 0.25f}, {"vcf_mod", 0.45f}, {"e1_decay", 0.42f}, {"e2_decay", 0.55f}, {"e2_sustain", 0.0f}, {"e2_release", 0.4f}, {"vary", 0.55f}, {"vary_seed", 2048}, {"rnd_count", 3}, {"m1_src", 12}, {"m1_dst", 17}, {"m1_amt", 0.4f}, {"dly_time", 6}, {"dly_fb", 0.45f}, {"fx_send", 0.35f}, {"fx_balance", 0.25f}, {nullptr, 0}}},
    {"sample and hold", {{"osc1_level", 0.5f}, {"osc2_wave", 2}, {"osc2_level", 0.4f}, {"cutoff", 0.45f}, {"resonance", 0.55f}, {"vcf_src", 1}, {"lfo1_wave", 2}, {"lfo1_rate", 0.62f}, {"vcf_mod", 0.45f}, {"e2_release", 0.3f}, {"fx_send", 0.3f}, {"fx_balance", 0.4f}, {nullptr, 0}}},
    {"sweep noise", {{"level", 1.0f}, {"osc1_wave", 5}, {"osc1_level", 0.6f}, {"cutoff", 0.3f}, {"resonance", 0.6f}, {"type_st", 6}, {"lfo1_wave", 6}, {"lfo1_rate", 0.15f}, {"vcf_src", 1}, {"vcf_mod", 0.6f}, {"e2_attack", 0.6f}, {"e2_release", 0.7f}, {"fx_send", 0.5f}, {"fx_balance", 0.9f}, {"rv_size", 0.9f}, {nullptr, 0}}},
    {"laser", {{"level", 0.6f}, {"osc1_wave", 2}, {"osc1_level", 0.38f}, {"pitch_src", 0}, {"pitch_mod", 0.6f}, {"e1_decay", 0.42f}, {"mod_osc", 1}, {"cutoff", 0.8f}, {"e2_decay", 0.5f}, {"e2_sustain", 0.0f}, {"e2_release", 0.2f}, {"dly_time", 6}, {"dly_fb", 0.4f}, {"fx_send", 0.3f}, {"fx_balance", 0.3f}, {nullptr, 0}}},
    {"wind", {{"osc1_wave", 5}, {"osc1_level", 0.7f}, {"type_st", 7}, {"cutoff", 0.55f}, {"resonance", 0.7f}, {"lfo1_wave", 7}, {"lfo1_rate", 0.25f}, {"vcf_src", 1}, {"vcf_mod", 0.4f}, {"delta", 0.3f}, {"e2_attack", 0.7f}, {"e2_release", 0.75f}, {"fx_send", 0.4f}, {"fx_balance", 0.95f}, {nullptr, 0}}},
    {"freeze cloud", {{"osc1_wave", 4}, {"osc1_level", 0.5f}, {"osc2_level", 0.3f}, {"osc2_tune", 7}, {"osc2_fine", 9}, {"cutoff", 0.6f}, {"e2_attack", 0.5f}, {"e2_release", 0.7f}, {"fx_send", 0.6f}, {"fx_balance", 0.5f}, {"dly_fb", 0.6f}, {"dly_time", 10}, {"rv_size", 0.9f}, {nullptr, 0}}},
    {"grit", {{"osc1_wave", 2}, {"osc1_level", 0.55f}, {"osc2_wave", 2}, {"osc2_level", 0.5f}, {"osc2_tune", -5}, {"drv_mode", 3}, {"drv_amount", 0.6f}, {"cutoff", 0.5f}, {"resonance", 0.4f}, {"e1_decay", 0.45f}, {"vcf_mod", 0.35f}, {"voice_mode", 1}, {nullptr, 0}}},
};
constexpr int kNumFactory = int(sizeof(kFactory) / sizeof(kFactory[0]));

}  // namespace tpre
