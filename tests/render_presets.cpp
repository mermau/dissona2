// Suona ogni preset di fabbrica attraverso il processore completo e salva un WAV per preset,
// controllando che non sia muto, non saturi e non contenga valori non finiti.
#include "PluginProcessor.h"

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 16, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();
    const double sr = 48000.0; const int bs = 256;
    int failures = 0;

    // tutti gli id dei preset devono esistere
    {
        DsProcessor p;
        for (auto& [id, v] : ds::presets::neutral()) if (! p.apvts.getParameter (id)) { std::printf ("id sconosciuto: %s\n", id); ++failures; }
        for (auto& pr : ds::presets::factory())
            for (auto& [id, v] : pr.values) if (! p.apvts.getParameter (id)) { std::printf ("id sconosciuto in %s: %s\n", pr.name, id); ++failures; }
    }

    for (int pi = 0; pi < (int) ds::presets::factory().size(); ++pi)
    {
        DsProcessor proc;
        proc.enableAllBuses();
        proc.setPlayConfigDetails (2, 2, sr, bs);
        proc.prepareToPlay (sr, bs);
        proc.setCurrentProgram (pi);
        const bool ext = juce::String (ds::presets::factory()[(size_t) pi].name).contains ("esterno");
        const int total = (int) (sr * 6.0);
        juce::AudioBuffer<float> out (2, total);
        juce::Random rnd (1);
        // frase: accordo (POLY) o note singole, poi rilascio
        struct Ev { double t; int note; bool on; };
        std::vector<Ev> evs { { 0.1, 48, true }, { 0.1, 55, true }, { 0.1, 60, true }, { 0.1, 64, true },
                              { 1.6, 48, false }, { 1.6, 55, false }, { 1.6, 60, false }, { 1.6, 64, false },
                              { 2.2, 60, true }, { 2.9, 60, false }, { 3.0, 67, true }, { 3.7, 67, false },
                              { 3.8, 72, true }, { 4.8, 72, false } };
        float peak = 0.0f; double sum = 0.0; bool finite = true;
        for (int pos = 0; pos < total; pos += bs)
        {
            juce::AudioBuffer<float> buf (2, bs);
            buf.clear();
            if (ext) for (int i = 0; i < bs; ++i) { const float v = 0.3f * (rnd.nextFloat() * 2.0f - 1.0f); buf.setSample (0, i, v); buf.setSample (1, i, v); }
            juce::MidiBuffer midi;
            for (auto& e : evs)
            {
                const int at = (int) (e.t * sr) - pos;
                if (at >= 0 && at < bs) midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, (juce::uint8) 100) : juce::MidiMessage::noteOff (1, e.note), at);
            }
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, buf, ch, 0, juce::jmin (bs, total - pos));
            for (int i = 0; i < bs; ++i) { const float v = buf.getSample (0, i); finite = finite && std::isfinite (v); peak = juce::jmax (peak, std::abs (v)); sum += (double) v * v; }
        }
        const double rms = std::sqrt (sum / total);
        const bool ok = finite && peak > 0.02f && peak < 1.5f;
        if (! ok) ++failures;
        std::printf ("%-32s picco %.3f  rms %.3f  %s\n", proc.getProgramName (pi).toRawUTF8(), peak, rms, ok ? "ok" : "PROBLEMA");
        writeWav (dir.getChildFile (juce::String (pi + 1).paddedLeft ('0', 2) + " " + proc.getProgramName (pi) + ".wav"), out, sr);
    }

    // riposo: senza note il motore deve dormire
    {
        DsProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, bs);
        proc.prepareToPlay (sr, bs);
        juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi;
        for (int b = 0; b < 400; ++b) { buf.clear(); proc.processBlock (buf, midi); }
        std::printf ("motore a riposo senza note: %s\n", proc.engineSleeping() ? "si" : "NO");
        if (! proc.engineSleeping()) ++failures;
    }
    std::printf (failures ? "%d PROBLEMI\n" : "Tutti i preset ok\n", failures);
    return failures ? 1 : 0;
}
