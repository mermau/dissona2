#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/DS2Engine.h"
#include "Params.h"
#include "Presets.h"

class DsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DsProcessor();
    ~DsProcessor() override;

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

    // i preset di fabbrica sono anche i "programmi" della DAW
    int getNumPrograms() override { return (int) ds::presets::factory().size(); }
    int getCurrentProgram() override { return juce::jmax (0, factoryIndex); }
    void setCurrentProgram (int index) override { loadFactory (index); }
    const juce::String getProgramName (int index) override;
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

    // --- Preset ------------------------------------------------------------------
    void loadFactory (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::File&);
    static juce::File userPresetFolder();
    juce::Array<juce::File> userPresets() const;
    juce::String currentPresetName() const { return presetName; }
    bool presetModified() const;                    // modificato rispetto al preset caricato
    void stepPreset (int delta);                    // ◀ ▶ tra fabbrica e preset utente
    // confronto A/B: due versioni del suono
    void toggleAB();
    void copyCurrentToOther();
    bool isOnB() const { return abSlot == 1; }

    float lfoLed (int which) const { return which == 1 ? led1.load() : led2.load(); }
    float wheelBend() const { return midiBend.load(); }
    bool  engineSleeping() const { return sleepingFlag.load(); }
    uint32_t instanceSeed() const { return seed.load(); }
    void newInstanceSeed() { seed.store ((uint32_t) juce::Random::getSystemRandom().nextInt() | 1u); seedChanged.store (true); }

private:
    void timerCallback() override;
    void readParams();
    void handleMidi (const juce::MidiMessage&);
    juce::ValueTree soundState() const;             // solo i parametri del suono
    void applySoundState (const juce::ValueTree&);
    void applyValues (const std::vector<std::pair<const char*, float>>&);
    void markLoaded (const juce::String& name, int factory);
    static bool isGlobal (const juce::String& id);

    ds2::Ds2Engine engine;
    ds2::EngineParams ep;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> overs;
    juce::dsp::Oversampling<float>* oversampler = nullptr;
    double hostRate = 48000.0;
    int factorFor (bool high) const;
    void selectFactor (int factor);
    juce::AudioBuffer<float> work;          // 2 canali: principale, poly
    std::vector<float> hiMain, hiPoly, hiExt;
    int osFactor = 2;

    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnIndex { -1 };
    std::atomic<float> led1 { 0.0f }, led2 { 0.0f }, midiBend { 0.0f };
    std::atomic<bool> sleepingFlag { false };
    std::atomic<uint32_t> seed { 0 };
    std::atomic<bool> seedChanged { false };      // il nuovo seme è applicato dal thread audio

    // valori arrivati da un CC appreso: usati subito dal motore, passati alla DAW dal thread principale
    struct ParamRef { std::atomic<float>* raw; juce::RangedAudioParameter* param; int index; };
    std::unordered_map<std::string, ParamRef> raw;
    std::vector<std::unique_ptr<std::atomic<float>>> pendingCC;   // normalizzato, NaN = nessuno
    float rawv (const char* id) const;

    // preset
    juce::String presetName { "Init (posizione neutra)" };
    int factoryIndex = 0;
    juce::File userFile;
    std::vector<float> loadedSnapshot;               // valori normalizzati al caricamento
    std::array<juce::ValueTree, 2> abState;
    int abSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DsProcessor)
};
