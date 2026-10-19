#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/DS2Engine.h"
#include "Params.h"

class DsProcessor : public juce::AudioProcessor
{
public:
    DsProcessor();
    ~DsProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // --- MIDI Learn -----------------------------------------------------------
    void startLearn (int paramIndex) { learnIndex.store (paramIndex); }
    void cancelLearn() { learnIndex.store (-1); }
    int  learningIndex() const { return learnIndex.load(); }
    int  ccForParam (int paramIndex) const;
    void clearMapping (int paramIndex);
    void resetDefaultMapping();
    int  paramIndexOf (const juce::String& id) const;

    float lfoLed (int which) const { return which == 1 ? led1.load() : led2.load(); }
    float wheelBend() const { return midiBend.load(); }

private:
    void readParams();
    void handleMidi (const juce::MidiMessage&);

    ds2::Ds2Engine engine;
    ds2::EngineParams ep;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> overs;
    juce::dsp::Oversampling<float>* oversampler = nullptr;
    double hostRate = 48000.0;
    int factorFor (bool high) const;
    void selectFactor (int factor);
    juce::AudioBuffer<float> work;          // 2 canali: principale, poly
    std::vector<float> hiMain, hiPoly;
    int osFactor = 2;

    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnIndex { -1 };
    std::atomic<float> led1 { 0.0f }, led2 { 0.0f }, midiBend { 0.0f };

    std::unordered_map<std::string, std::atomic<float>*> raw;
    float rawv (const char* id) const { return raw.at (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsProcessor)
};
