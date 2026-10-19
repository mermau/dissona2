#pragma once
// Preset di fabbrica di DISSONA 2. I valori sono scritti nella scala del pannello e convertiti
// nei valori dei parametri dalle funzioni qui sotto.
#include <utility>
#include <vector>

namespace ds::presets
{
// manopola 0 … 10
inline float K (float x) { return x / 10.0f; }
// manopola bipolare −5 … +5 (PITCH, HIGH/LOW PASS)
inline float B (float x) { return x / 5.0f; }
// cursore ADSR: 0 = tutto in basso, 1 … 7 come la scala serigrafata
inline float S (float x) { return x <= 0.0f ? 0.0f : 0.12f + (x - 1.0f) / 6.0f * 0.73f; }
// cursore del LFO's MIXER: 0 = tutto a sinistra, 1 … 7 come la scala serigrafata
inline float M (float x) { return x <= 0.0f ? 0.0f : 0.12f + (x - 1.0f) / 6.0f * 0.75f; }

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // id → valore del parametro (non normalizzato)
};

// Posizione neutra (manuale Crumar, "Neutral setting"): ogni preset parte da qui
inline const std::vector<std::pair<const char*, float>>& neutral()
{
    static const std::vector<std::pair<const char*, float>> v {
        { "mx_o1_amt", 0 }, { "mx_o1_src", 0 }, { "mx_o2_amt", 0 }, { "mx_o2_src", 0 },
        { "mx_vcf_amt", 0 }, { "mx_vcf_src", 0 }, { "mx_vca_amt", 0 }, { "mx_vca_src", 0 },
        { "mx_pw_amt", 0 }, { "mx_pw_src", 0 },
        { "lfo1_rate", K (5) }, { "lfo1_wave", 1 }, { "lfo1_delay", 0 },
        { "lfo2_rate", K (5) }, { "lfo2_wave", 1 }, { "lfo2_delay", 0 }, { "lfo_delay", K (3) },
        { "o1_vol", K (7) }, { "o1_pitch", 0 }, { "o1_range", 2 }, { "o1_wave", 0 },
        { "o2_vol", 0 }, { "o2_pitch", 0 }, { "o2_range", 2 }, { "o2_wave", 1 }, { "kbd_ctrl", 0 },
        { "vcf_env", 0 }, { "vcf_cut", K (10) }, { "vcf_res", 0 },
        { "vcf_a", 0 }, { "vcf_d", S (3) }, { "vcf_s", S (7) }, { "vcf_r", S (1) },
        { "vca_env", K (10) }, { "vca_a", 0 }, { "vca_d", S (3) }, { "vca_s", S (7) }, { "vca_r", S (1) }, { "vca_in", 0 },
        { "noise_vol", 0 }, { "noise_pink", 0 },
        { "poly_lfo_amt", 0 }, { "poly_lfo_src", 0 }, { "poly_high", 0 }, { "poly_low", 0 },
        { "poly_vol", K (7) }, { "poly_on", 0 }, { "poly_oct", 1 }, { "poly_pitch", 0 },
        { "head_main", 0 }, { "volume", K (6) },
        { "glide_on", 0 }, { "glide_range", 0 }, { "glide_speed", K (5) },
        { "retrigger", 0 }, { "vca_sw_mode", 0 }, { "priority", 0 }, { "pedal_range", 2 },
        { "ext_vol", 0 }, { "ext_open", 0 },
    };
    return v;
}

inline const std::vector<Preset>& factory()
{
    static const std::vector<Preset> list {
        { "Init (posizione neutra)", {} },
        { "Basso jazz", {
            { "o1_range", 1 }, { "o1_wave", 0 }, { "o1_vol", K (7) },
            { "o2_range", 1 }, { "o2_wave", 2 }, { "o2_vol", K (5) },
            { "vcf_cut", K (3) }, { "vcf_res", K (7) }, { "vcf_env", K (7) },
            { "vcf_a", 0 }, { "vcf_d", S (3) }, { "vcf_s", 0 }, { "vcf_r", S (2) },
            { "vca_a", 0 }, { "vca_d", S (4) }, { "vca_s", S (4) }, { "vca_r", S (2) }, { "volume", K (8) } } },
        { "Archi POLY", {
            { "o1_vol", 0 }, { "poly_on", 1 }, { "poly_vol", K (8.5f) }, { "poly_high", B (1.5f) },
            { "poly_lfo_src", 1 }, { "poly_lfo_amt", M (3) }, { "lfo1_rate", K (5.5f) },
            { "vcf_cut", K (7.5f) },
            { "vca_a", S (4) }, { "vca_d", S (1) }, { "vca_s", S (7) }, { "vca_r", S (5) } } },
        { "Archi e synth", {
            { "o1_vol", K (5) }, { "o1_wave", 0 }, { "poly_on", 1 }, { "poly_vol", K (7) },
            { "poly_lfo_src", 1 }, { "poly_lfo_amt", M (3) }, { "lfo1_rate", K (5.5f) },
            { "vcf_cut", K (6.5f) }, { "vcf_res", K (2) }, { "vcf_env", K (3) },
            { "vcf_a", S (3) }, { "vcf_d", S (4) }, { "vcf_s", S (5) }, { "vcf_r", S (4) },
            { "vca_a", S (3) }, { "vca_s", S (7) }, { "vca_r", S (4) } } },
        { "Lead PWM", {
            { "o1_wave", 3 }, { "mx_pw_src", 1 }, { "mx_pw_amt", M (5) }, { "lfo1_rate", K (3) },
            { "vcf_cut", K (6) }, { "vcf_res", K (3) }, { "vcf_env", K (3) },
            { "vcf_a", 0 }, { "vcf_d", S (5) }, { "vcf_s", S (4) }, { "vcf_r", S (3) },
            { "vca_a", 0 }, { "vca_d", 0 }, { "vca_s", S (7) }, { "vca_r", S (3) },
            { "glide_on", 1 }, { "glide_speed", K (6) } } },
        { "Ottoni", {
            { "o1_wave", 0 }, { "o2_wave", 0 }, { "o2_vol", K (6) }, { "o2_pitch", B (0.1f) },
            { "vcf_cut", K (3.5f) }, { "vcf_res", K (1.5f) }, { "vcf_env", K (6) },
            { "vcf_a", S (3) }, { "vcf_d", S (4) }, { "vcf_s", S (5) }, { "vcf_r", S (3) },
            { "vca_a", S (2) }, { "vca_d", 0 }, { "vca_s", S (7) }, { "vca_r", S (3) } } },
        { "Fischio intonato", {
            { "o1_vol", 0 }, { "vcf_cut", K (4.75f) }, { "vcf_res", K (9.2f) }, { "kbd_ctrl", 1 }, { "volume", K (2.5f) },
            { "mx_vcf_src", 1 }, { "mx_vcf_amt", M (1) }, { "lfo1_rate", K (5.5f) },
            { "lfo1_delay", 1 }, { "lfo_delay", K (5) },
            { "vca_a", S (2) }, { "vca_d", 0 }, { "vca_s", S (7) }, { "vca_r", S (4) } } },
        { "Bloop S&H", {
            { "o1_wave", 2 }, { "mx_vcf_src", 2 }, { "mx_vcf_amt", M (6) }, { "lfo2_wave", 0 }, { "lfo2_rate", K (6) },
            { "vcf_cut", K (3) }, { "vcf_res", K (8) }, { "vcf_env", K (2) },
            { "vcf_a", 0 }, { "vcf_d", S (2) }, { "vcf_s", 0 }, { "vcf_r", S (1) },
            { "vca_a", 0 }, { "vca_d", S (3) }, { "vca_s", S (7) }, { "vca_r", S (2) } } },
        { "Scala staircase", {
            { "o1_range", 1 }, { "o1_wave", 0 }, { "mx_o1_src", 2 }, { "mx_o1_amt", M (4) },
            { "lfo2_wave", 3 }, { "lfo2_rate", K (5.5f) },
            { "vcf_cut", K (6) }, { "vcf_res", K (5) },
            { "vca_a", 0 }, { "vca_s", S (7) }, { "vca_r", S (3) }, { "volume", K (8) } } },
        { "Campane", {
            { "o1_wave", 1 }, { "o2_wave", 2 }, { "o2_range", 3 }, { "o2_vol", K (5) }, { "o2_pitch", B (3.9f) },
            { "vcf_cut", K (7) }, { "vcf_res", K (4) }, { "vcf_env", K (5) },
            { "vcf_a", 0 }, { "vcf_d", S (5) }, { "vcf_s", 0 }, { "vcf_r", S (5) },
            { "vca_a", 0 }, { "vca_d", S (6) }, { "vca_s", 0 }, { "vca_r", S (6) } } },
        { "Organo POLY", {
            { "o1_vol", 0 }, { "poly_on", 1 }, { "poly_vol", K (8) }, { "poly_oct", 0 },
            { "poly_low", B (1.5f) }, { "poly_high", B (1) }, { "vcf_cut", K (9) },
            { "vca_a", 0 }, { "vca_d", 0 }, { "vca_s", S (7) }, { "vca_r", S (2) } } },
        { "Tremolo POLY", {
            { "o1_vol", 0 }, { "poly_on", 1 }, { "poly_vol", K (8) },
            { "mx_vca_src", 1 }, { "mx_vca_amt", M (4) }, { "lfo1_rate", K (6) },
            { "vcf_cut", K (8) },
            { "vca_a", S (1) }, { "vca_d", 0 }, { "vca_s", S (6) }, { "vca_r", S (3) }, { "volume", K (8) } } },
        { "Vento", {
            { "o1_vol", 0 }, { "noise_vol", K (8) }, { "vcf_cut", K (4.5f) }, { "vcf_res", K (8.5f) },
            { "mx_vcf_src", 1 }, { "mx_vcf_amt", M (5) }, { "lfo1_rate", K (1.5f) },
            { "vca_a", S (5) }, { "vca_d", 0 }, { "vca_s", S (7) }, { "vca_r", S (6) }, { "volume", K (8) } } },
        { "Filtro per ingresso esterno", {
            { "o1_vol", 0 }, { "ext_vol", K (8) }, { "ext_open", 1 },
            { "vcf_cut", K (5) }, { "vcf_res", K (6) },
            { "mx_vcf_src", 1 }, { "mx_vcf_amt", M (5) }, { "lfo1_rate", K (4) } } },
    };
    return list;
}
}
