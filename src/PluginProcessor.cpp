#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ds;

DsProcessor::DsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Uscita", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DISSONA2", createLayout())
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[rp->paramID.toStdString()] = apvts.getRawParameterValue (rp->paramID);
    resetDefaultMapping();
}

int DsProcessor::paramIndexOf (const juce::String& id) const
{
    const auto& ps = getParameters();
    for (int i = 0; i < ps.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
            if (rp->paramID == id) return i;
    return -1;
}

void DsProcessor::resetDefaultMapping()
{
    for (auto& c : ccMap) c.store (-1);
    // Assegnazioni predefinite (tutto il resto con MIDI Learn: clic destro su un comando)
    ccMap[74].store (paramIndexOf (pid::vcfCut));      // brightness  -> CUTOFF FILTER
    ccMap[71].store (paramIndexOf (pid::vcfRes));      // resonance   -> RESONANCE
    ccMap[1].store  (paramIndexOf (pid::mxO1Amt));     // mod wheel   -> LFO mixer OSC 1 (vibrato)
    ccMap[7].store  (paramIndexOf (pid::volume));      // volume      -> VOLUME
    ccMap[5].store  (paramIndexOf (pid::glideSpeed));  // portamento  -> SPEED GLIDE
    ccMap[65].store (paramIndexOf (pid::glideOn));     // portamento on/off -> GLIDE IN
    ccMap[73].store (paramIndexOf (pid::vcaA));        // attack      -> VCA A
    ccMap[72].store (paramIndexOf (pid::vcaR));        // release     -> VCA R
    ccMap[75].store (paramIndexOf (pid::vcfD));        // decay       -> VCF D
}

int DsProcessor::ccForParam (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc) if (ccMap[(size_t) cc].load() == paramIndex) return cc;
    return -1;
}

void DsProcessor::clearMapping (int paramIndex)
{
    for (auto& c : ccMap) if (c.load() == paramIndex) c.store (-1);
}

bool DsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    for (size_t st = 1; st <= 2; ++st)
    {
        overs[st] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, st, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        overs[st]->initProcessing ((size_t) samplesPerBlock);
    }
    selectFactor (factorFor (rawv (pid::quality) > 0.5f));
    setLatencySamples (oversampler != nullptr ? (int) std::round (0.5 * oversampler->getLatencyInSamples()) : 0);
    work.setSize (2, samplesPerBlock);
    hiMain.assign ((size_t) samplesPerBlock * 4, 0.0f);
    hiPoly.assign ((size_t) samplesPerBlock * 4, 0.0f);
    readParams();
    engine.setParams (ep);
    keyboardState.reset();
}

// Qualità normale: il motore gira a ~88–96 kHz; alta: ~176–192 kHz
int DsProcessor::factorFor (bool high) const
{
    if (hostRate <= 50000.0)  return high ? 4 : 2;
    if (hostRate <= 100000.0) return high ? 2 : 1;
    return 1;
}

void DsProcessor::selectFactor (int factor)
{
    osFactor = factor;
    oversampler = factor == 4 ? overs[2].get() : (factor == 2 ? overs[1].get() : nullptr);
    if (oversampler) oversampler->reset();
    engine.prepare (hostRate * factor);
}

void DsProcessor::readParams()
{
    auto i = [this] (const char* id) { return (int) rawv (id); };
    auto b = [this] (const char* id) { return rawv (id) > 0.5f; };
    ep.mxO1Amt = rawv (pid::mxO1Amt);   ep.mxO1Src = i (pid::mxO1Src);
    ep.mxO2Amt = rawv (pid::mxO2Amt);   ep.mxO2Src = i (pid::mxO2Src);
    ep.mxVcfAmt = rawv (pid::mxVcfAmt); ep.mxVcfSrc = i (pid::mxVcfSrc);
    ep.mxVcaAmt = rawv (pid::mxVcaAmt); ep.mxVcaSrc = i (pid::mxVcaSrc);
    ep.mxPwAmt = rawv (pid::mxPwAmt);   ep.mxPwSrc = i (pid::mxPwSrc);
    ep.lfo1Rate = rawv (pid::lfo1Rate); ep.lfo1Wave = i (pid::lfo1Wave); ep.lfo1Delay = b (pid::lfo1Delay);
    ep.lfo2Rate = rawv (pid::lfo2Rate); ep.lfo2Wave = i (pid::lfo2Wave); ep.lfo2Delay = b (pid::lfo2Delay);
    ep.lfoDelay = rawv (pid::lfoDelay);
    ep.o1Vol = rawv (pid::o1Vol); ep.o1Pitch = rawv (pid::o1Pitch); ep.o1Range = i (pid::o1Range); ep.o1Wave = i (pid::o1Wave);
    ep.o2Vol = rawv (pid::o2Vol); ep.o2Pitch = rawv (pid::o2Pitch); ep.o2Range = i (pid::o2Range); ep.o2Wave = i (pid::o2Wave);
    ep.kbdCtrl = b (pid::kbdCtrl);
    ep.vcfEnv = rawv (pid::vcfEnv); ep.vcfCut = rawv (pid::vcfCut); ep.vcfRes = rawv (pid::vcfRes);
    ep.vcfA = rawv (pid::vcfA); ep.vcfD = rawv (pid::vcfD); ep.vcfS = rawv (pid::vcfS); ep.vcfR = rawv (pid::vcfR);
    ep.vcaEnv = rawv (pid::vcaEnv);
    ep.vcaA = rawv (pid::vcaA); ep.vcaD = rawv (pid::vcaD); ep.vcaS = rawv (pid::vcaS); ep.vcaR = rawv (pid::vcaR);
    ep.vcaIn = b (pid::vcaIn); ep.vcaSwMode = i (pid::vcaSwMode);
    ep.noiseVol = rawv (pid::noiseVol); ep.noisePink = b (pid::noisePink);
    ep.polyLfoAmt = rawv (pid::polyLfoAmt); ep.polyLfoSrc = i (pid::polyLfoSrc);
    ep.polyHigh = rawv (pid::polyHigh); ep.polyLow = rawv (pid::polyLow);
    ep.polyVol = rawv (pid::polyVol); ep.polyOn = b (pid::polyOn);
    ep.polyOct = i (pid::polyOct); ep.polyPitch = rawv (pid::polyPitch);
    ep.headphones = b (pid::headMain); ep.volume = rawv (pid::volume);
    ep.glideOn = b (pid::glideOn); ep.glideLong = b (pid::glideLong); ep.glideSpeed = rawv (pid::glideSpeed);
    ep.bend = juce::jlimit (-1.0f, 1.0f, rawv (pid::bend) + midiBend.load());
    ep.bendRange = 3.0f;   // la leva dell'originale: ±terza minore
    ep.retrigger = b (pid::retrigger);
    ep.originalRange = i (pid::keyRange) == 1;
    ep.polySplit = b (pid::polySplit);
}

void DsProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())            engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())
    {
        // la rotella MIDI muove la leva BENDING; l'ampiezza si regola nelle impostazioni
        const float w = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        const float semis = rawv (pid::bendRange);
        midiBend.store (w * semis / 3.0f);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        const int learn = learnIndex.load();
        if (learn >= 0 && cc != 64)
        {
            clearMapping (learn);
            ccMap[(size_t) cc].store (learn);
            learnIndex.store (-1);
        }
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0)
        {
            if (auto* p = getParameters()[idx])
                p->setValueNotifyingHost ((float) val / 127.0f);
        }
        else if (cc == 64)
            engine.setSustainPedal (val >= 64);
    }
}

void DsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numOut = getTotalNumOutputChannels();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (const int want = factorFor (rawv (pid::quality) > 0.5f); want != osFactor)
        selectFactor (want);

    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);
    if ((int) hiMain.size() < n * osFactor) { hiMain.resize ((size_t) (n * osFactor)); hiPoly.resize ((size_t) (n * osFactor)); }
    work.clear();

    readParams();
    engine.setParams (ep);

    int pos = 0;
    auto run = [&] (int to)
    {
        if (to > pos)
        {
            const int a = pos * osFactor, len = (to - pos) * osFactor;
            engine.process (hiMain.data() + a, hiPoly.data() + a, len);
            pos = to;
        }
    };
    for (const auto meta : midi)
    {
        run (juce::jlimit (0, n, meta.samplePosition));
        handleMidi (meta.getMessage());
        readParams();          // un CC appreso può aver cambiato un parametro
        engine.setParams (ep);
    }
    run (n);

    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
    if (oversampler)
    {
        auto hi = oversampler->processSamplesUp (block);   // solo per ottenere i buffer interni della giusta lunghezza
        std::copy (hiMain.begin(), hiMain.begin() + n * osFactor, hi.getChannelPointer (0));
        std::copy (hiPoly.begin(), hiPoly.begin() + n * osFactor, hi.getChannelPointer (1));
        oversampler->processSamplesDown (block);
    }
    else
    {
        std::copy (hiMain.begin(), hiMain.begin() + n, work.getWritePointer (0));
        std::copy (hiPoly.begin(), hiPoly.begin() + n, work.getWritePointer (1));
    }

    // uscita POLY separata: principale a sinistra, POLY a destra; altrimenti mono su entrambi
    const bool split = ep.polySplit && ep.polyOn;
    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        if (split) buffer.copyFrom (1, 0, work, 1, 0, n);
        else       buffer.copyFrom (1, 0, work, 0, 0, n);
        for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    }
    else if (numOut == 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        if (split) buffer.addFrom (0, 0, work, 1, 0, n);
    }

    led1.store (engine.lfoLed (1));
    led2.store (engine.lfoLed (2));
}

void DsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto map = state.getOrCreateChildWithName ("MIDIMAP", nullptr);
    map.removeAllProperties (nullptr);
    for (int cc = 0; cc < 128; ++cc)
    {
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[idx]))
                map.setProperty ("cc" + juce::String (cc), rp->paramID, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void DsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        auto map = state.getChildWithName ("MIDIMAP");
        if (map.isValid())
        {
            for (auto& c : ccMap) c.store (-1);
            for (int i = 0; i < map.getNumProperties(); ++i)
            {
                const auto name = map.getPropertyName (i).toString();
                const int cc = name.substring (2).getIntValue();
                if (cc >= 0 && cc < 128)
                    ccMap[(size_t) cc].store (paramIndexOf (map.getProperty (name).toString()));
            }
            state.removeChild (map, nullptr);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* DsProcessor::createEditor() { return new DsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DsProcessor(); }
