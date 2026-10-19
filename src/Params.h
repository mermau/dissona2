#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ds::pid
{
// LFO's MIXER
inline constexpr const char* mxO1Amt  = "mx_o1_amt";
inline constexpr const char* mxO1Src  = "mx_o1_src";
inline constexpr const char* mxO2Amt  = "mx_o2_amt";
inline constexpr const char* mxO2Src  = "mx_o2_src";
inline constexpr const char* mxVcfAmt = "mx_vcf_amt";
inline constexpr const char* mxVcfSrc = "mx_vcf_src";
inline constexpr const char* mxVcaAmt = "mx_vca_amt";
inline constexpr const char* mxVcaSrc = "mx_vca_src";
inline constexpr const char* mxPwAmt  = "mx_pw_amt";
inline constexpr const char* mxPwSrc  = "mx_pw_src";
// LFO
inline constexpr const char* lfo1Rate  = "lfo1_rate";
inline constexpr const char* lfo1Wave  = "lfo1_wave";
inline constexpr const char* lfo1Delay = "lfo1_delay";
inline constexpr const char* lfo2Rate  = "lfo2_rate";
inline constexpr const char* lfo2Wave  = "lfo2_wave";
inline constexpr const char* lfo2Delay = "lfo2_delay";
inline constexpr const char* lfoDelay  = "lfo_delay";
// OSC
inline constexpr const char* o1Vol   = "o1_vol";
inline constexpr const char* o1Pitch = "o1_pitch";
inline constexpr const char* o1Range = "o1_range";
inline constexpr const char* o1Wave  = "o1_wave";
inline constexpr const char* o2Vol   = "o2_vol";
inline constexpr const char* o2Pitch = "o2_pitch";
inline constexpr const char* o2Range = "o2_range";
inline constexpr const char* o2Wave  = "o2_wave";
inline constexpr const char* kbdCtrl = "kbd_ctrl";
// VCF
inline constexpr const char* vcfEnv = "vcf_env";
inline constexpr const char* vcfCut = "vcf_cut";
inline constexpr const char* vcfRes = "vcf_res";
inline constexpr const char* vcfA   = "vcf_a";
inline constexpr const char* vcfD   = "vcf_d";
inline constexpr const char* vcfS   = "vcf_s";
inline constexpr const char* vcfR   = "vcf_r";
// VCA
inline constexpr const char* vcaEnv = "vca_env";
inline constexpr const char* vcaA   = "vca_a";
inline constexpr const char* vcaD   = "vca_d";
inline constexpr const char* vcaS   = "vca_s";
inline constexpr const char* vcaR   = "vca_r";
inline constexpr const char* vcaIn  = "vca_in";
// NOISE
inline constexpr const char* noiseVol  = "noise_vol";
inline constexpr const char* noisePink = "noise_pink";
// POLY
inline constexpr const char* polyLfoAmt = "poly_lfo_amt";
inline constexpr const char* polyLfoSrc = "poly_lfo_src";
inline constexpr const char* polyHigh   = "poly_high";
inline constexpr const char* polyLow    = "poly_low";
inline constexpr const char* polyVol    = "poly_vol";
inline constexpr const char* polyOn     = "poly_on";
inline constexpr const char* polyOct    = "poly_oct";
inline constexpr const char* polyPitch  = "poly_pitch";
// uscita, tastiera
inline constexpr const char* headMain   = "head_main";
inline constexpr const char* volume     = "volume";
inline constexpr const char* glideOn    = "glide_on";
inline constexpr const char* glideLong  = "glide_range";
inline constexpr const char* glideSpeed = "glide_speed";
inline constexpr const char* bend       = "bend";
// impostazioni
inline constexpr const char* bendRange = "bend_range";
inline constexpr const char* keyRange  = "key_range";
inline constexpr const char* retrigger = "retrigger";
inline constexpr const char* vcaSwMode = "vca_sw_mode";
inline constexpr const char* polySplit = "poly_split";
inline constexpr const char* quality   = "quality";
}

namespace ds
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1;
    auto knob = [&] (const char* id, const char* name, float def, float lo = 0.0f, float hi = 1.0f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, v }, String::fromUTF8 (name), NormalisableRange<float> (lo, hi, 0.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([lo] (float x, int)
            {
                // scale del pannello: 0 … 10; manopole bipolari −5 … +5
                return lo < 0.0f ? String (x * 5.0f, 1) : String (x * 10.0f, 1);
            })));
    };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, v }, String::fromUTF8 (name), items, def)); };
    auto toggle = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, v }, String::fromUTF8 (name), def)); };

    const StringArray src { "OUT", "1", "2", "1+2" };
    const StringArray foot { "32'", "16'", "8'", "4'" };

    knob   (pid::mxO1Amt,  "LFO Mixer - OSC 1 range", 0.0f);
    choice (pid::mxO1Src,  "LFO Mixer - OSC 1", src, 0);
    knob   (pid::mxO2Amt,  "LFO Mixer - OSC 2 range", 0.0f);
    choice (pid::mxO2Src,  "LFO Mixer - OSC 2", src, 0);
    knob   (pid::mxVcfAmt, "LFO Mixer - VCF range", 0.0f);
    choice (pid::mxVcfSrc, "LFO Mixer - VCF", src, 0);
    knob   (pid::mxVcaAmt, "LFO Mixer - VCA range", 0.0f);
    choice (pid::mxVcaSrc, "LFO Mixer - VCA", src, 0);
    knob   (pid::mxPwAmt,  "LFO Mixer - Pulse width", 0.0f);
    choice (pid::mxPwSrc,  "LFO Mixer - Pulse width source", { "MAN.", "1", "2" }, 0);

    knob   (pid::lfo1Rate,  "LFO 1 - Rate", 0.5f);
    choice (pid::lfo1Wave,  "LFO 1 - Forma", { "Dente di sega inverso", "Triangolo", "Quadra", "Dente di sega" }, 1);
    toggle (pid::lfo1Delay, "LFO 1 - Delay IN", false);
    knob   (pid::lfo2Rate,  "LFO 2 - Rate", 0.5f);
    choice (pid::lfo2Wave,  "LFO 2 - Forma", { "S&H (random)", "Triangolo", "Quadra", "Staircase" }, 1);
    toggle (pid::lfo2Delay, "LFO 2 - Delay IN", false);
    knob   (pid::lfoDelay,  "LFO's Delay", 0.3f);

    knob   (pid::o1Vol,   "OSC 1 - Volume", 0.7f);
    knob   (pid::o1Pitch, "OSC 1 - Pitch", 0.0f, -1.0f, 1.0f);
    choice (pid::o1Range, "OSC 1 - Ottava", foot, 2);
    choice (pid::o1Wave,  "OSC 1 - Forma", { "Dente di sega", "Triangolo", "Quadra", "Impulso" }, 0);
    knob   (pid::o2Vol,   "OSC 2 - Volume", 0.0f);
    knob   (pid::o2Pitch, "OSC 2 - Pitch", 0.0f, -1.0f, 1.0f);
    choice (pid::o2Range, "OSC 2 - Ottava", foot, 2);
    choice (pid::o2Wave,  "OSC 2 - Forma", { "Dente di sega", "Triangolo", "Quadra" }, 1);
    toggle (pid::kbdCtrl, "Keyboard control IN", false);

    knob   (pid::vcfEnv, "VCF - ADSR range", 0.5f);
    knob   (pid::vcfCut, "VCF - Cutoff", 0.55f);
    knob   (pid::vcfRes, "VCF - Resonance", 0.0f);
    knob   (pid::vcfA,   "VCF - Attack", 0.0f);
    knob   (pid::vcfD,   "VCF - Decay", 0.55f);
    knob   (pid::vcfS,   "VCF - Sustain", 0.6f);
    knob   (pid::vcfR,   "VCF - Release", 0.2f);

    knob   (pid::vcaEnv, "VCA - ADSR range", 0.8f);
    knob   (pid::vcaA,   "VCA - Attack", 0.0f);
    knob   (pid::vcaD,   "VCA - Decay", 0.55f);
    knob   (pid::vcaS,   "VCA - Sustain", 0.8f);
    knob   (pid::vcaR,   "VCA - Release", 0.2f);
    toggle (pid::vcaIn,  "ADSR to VCA IN", false);

    knob   (pid::noiseVol,  "Noise - Volume", 0.0f);
    toggle (pid::noisePink, "Noise - Pink", false);

    knob   (pid::polyLfoAmt, "Poly - LFO amount", 0.0f);
    choice (pid::polyLfoSrc, "Poly - LFO", src, 0);
    knob   (pid::polyHigh,   "Poly - High pass (acuti)", 0.0f, -1.0f, 1.0f);
    knob   (pid::polyLow,    "Poly - Low pass (bassi)", 0.0f, -1.0f, 1.0f);
    knob   (pid::polyVol,    "Poly - Volume", 0.6f);
    toggle (pid::polyOn,     "Poly IN", false);
    choice (pid::polyOct,    "Poly - Octave", { "16'", "8'", "4'" }, 1);
    knob   (pid::polyPitch,  "Poly - Pitch", 0.0f, -1.0f, 1.0f);

    toggle (pid::headMain,   "Head / Main (Head)", false);
    knob   (pid::volume,     "Volume", 0.7f);
    toggle (pid::glideOn,    "Glide IN", false);
    toggle (pid::glideLong,  "Glide - Speed range IN", false);
    knob   (pid::glideSpeed, "Glide - Speed", 0.5f);
    knob   (pid::bend,       "Pitch bending", 0.0f, -1.0f, 1.0f);

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::bendRange, v }, "Pitch bend MIDI (semitoni)", 0, 12, 3));
    choice (pid::keyRange,  "Estensione tastiera", { "Completa (MIDI)", "44 tasti FA-DO (originale)" }, 0);
    toggle (pid::retrigger, "Riattacco in legato", false);
    choice (pid::vcaSwMode, "Deviatore VCF-VCA", { "Manuale Crumar", "Macchina Bassi" }, 0);
    toggle (pid::polySplit, "Uscita POLY separata", false);
    choice (pid::quality,   "Qualita", { "Normale (2x)", "Alta (4x)" }, 0);

    return { p.begin(), p.end() };
}
}
