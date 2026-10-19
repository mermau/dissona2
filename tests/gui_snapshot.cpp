// Rende l'interfaccia in PNG senza schermo (xvfb-run) e prova il processore con un accordo.
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "snapshot.png";
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    DsProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 100);
    float peak = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        midi.clear();
        peak = juce::jmax (peak, buf.getMagnitude (0, 0, 512));
    }
    std::printf ("picco uscita processore: %.3f\n", peak);

    // apprendimento MIDI: CC 20 sulla risonanza
    proc.startLearn (proc.paramIndexOf ("vcf_res"));
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 100), 0);
    proc.processBlock (buf, midi); midi.clear();
    std::printf ("CC appreso per vcf_res: %d, valore: %.3f\n", proc.ccForParam (proc.paramIndexOf ("vcf_res")),
                 proc.apvts.getRawParameterValue ("vcf_res")->load());

    juce::MemoryBlock mb; proc.getStateInformation (mb);
    DsProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
    std::printf ("dopo ricarica: CC per vcf_res = %d\n", p2.ccForParam (p2.paramIndexOf ("vcf_res")));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (1880 * scale), (int) (980 * scale));
    proc.keyboardState.noteOn (1, 60, 1.0f);
    proc.keyboardState.noteOn (1, 64, 1.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("salvato %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
