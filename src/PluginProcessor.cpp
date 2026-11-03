#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ds;

DsProcessor::DsProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Ingresso esterno", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Uscita", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DISSONA2", createLayout())
{
    const auto& ps = getParameters();
    pendingCC.reserve ((size_t) ps.size());
    for (int i = 0; i < ps.size(); ++i)
    {
        pendingCC.push_back (std::make_unique<std::atomic<float>> (std::numeric_limits<float>::quiet_NaN()));
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
            raw[rp->paramID.toStdString()] = { apvts.getRawParameterValue (rp->paramID), rp, i };
    }
    for (const char* id : pid::all)
        byPtr[id] = &raw.at (id);
    // ogni istanza è un esemplare diverso: il seme viene salvato con il progetto
    seed.store ((uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);
    seedChanged.store (true);
    resetDefaultMapping();
    loadFactory (0);
    startTimerHz (30);
}

DsProcessor::~DsProcessor() { stopTimer(); }

// valore di un parametro: un CC appena arrivato ha la precedenza finché la DAW non lo ha ricevuto
float DsProcessor::rawv (const char* id) const
{
    const auto it = byPtr.find (id);
    const auto& r = it != byPtr.end() ? *it->second : raw.at (id);
    const float pend = pendingCC[(size_t) r.index]->load();
    if (! std::isnan (pend)) return r.param->convertFrom0to1 (pend);
    return r.raw->load();
}

int DsProcessor::paramIndexOf (const juce::String& id) const
{
    const auto it = raw.find (id.toStdString());
    return it == raw.end() ? -1 : it->second.index;
}

void DsProcessor::resetDefaultMapping()
{
    for (auto& c : ccMap) c.store (-1);
    // Assegnazioni predefinite pensate per l'Arturia KeyLab (il resto con MIDI Learn)
    ccMap[74].store (paramIndexOf (pid::vcfCut));      // brightness  -> CUTOFF FILTER
    ccMap[71].store (paramIndexOf (pid::vcfRes));      // resonance   -> RESONANCE
    ccMap[1].store  (paramIndexOf (pid::mxO1Amt));     // mod wheel   -> LFO mixer OSC 1 (vibrato)
    ccMap[7].store  (paramIndexOf (pid::volume));      // volume      -> VOLUME
    ccMap[5].store  (paramIndexOf (pid::glideSpeed));  // portamento  -> SPEED GLIDE
    ccMap[65].store (paramIndexOf (pid::glideOn));     // portamento on/off -> GLIDE IN
    ccMap[73].store (paramIndexOf (pid::vcaA));        // attack      -> VCA A
    ccMap[72].store (paramIndexOf (pid::vcaR));        // release     -> VCA R
    ccMap[75].store (paramIndexOf (pid::vcfD));        // decay       -> VCF D
    ccMap[11].store (paramIndexOf (pid::pedal));       // expression  -> pedale del filtro
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
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
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
    work.setSize (2, samplesPerBlock);
    hiMain.assign ((size_t) samplesPerBlock * 4, 0.0f);
    hiPoly.assign ((size_t) samplesPerBlock * 4, 0.0f);
    hiExt.assign ((size_t) samplesPerBlock * 4, 0.0f);
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
    // la latenza dei filtri di sovracampionamento cambia con il fattore: la DAW deve saperlo
    const int lat = oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
    if (lat != getLatencySamples()) setLatencySamples (lat);
    engine.prepare (hostRate * factor);
    engine.setSeed (seed.load());
}

void DsProcessor::readParams()
{
    auto i = [this] (const char* id) { return (int) std::lround (rawv (id)); };
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
    ep.extVol = rawv (pid::extVol); ep.extOpen = b (pid::extOpen);
    ep.priority = i (pid::priority);
    ep.pedal = rawv (pid::pedal);
    ep.pedalOct = 2.0f * (float) i (pid::pedalRange);
    ep.tuneA4 = rawv (pid::tune);
    ep.tolerances = b (pid::tolerances);
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
        midiBend.store (w * rawv (pid::bendRange) / 3.0f);
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
        if (idx >= 0 && idx < (int) pendingCC.size())
            // il motore lo usa subito; la DAW lo riceve dal thread principale (timerCallback)
            pendingCC[(size_t) idx]->store ((float) val / 127.0f);
        else if (cc == 64)
            engine.setSustainPedal (val >= 64);
    }
}

void DsProcessor::timerCallback()
{
    const auto& ps = getParameters();
    for (size_t i = 0; i < pendingCC.size(); ++i)
    {
        const float v = pendingCC[i]->load();
        if (std::isnan (v)) continue;
        if (auto* p = ps[(int) i])
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
        // se nel frattempo è arrivato un valore nuovo resta in attesa per il prossimo giro
        float expected = v;
        pendingCC[i]->compare_exchange_strong (expected, std::numeric_limits<float>::quiet_NaN());
    }
}

void DsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (const int want = factorFor (rawv (pid::quality) > 0.5f); want != osFactor)
        selectFactor (want);
    if (seedChanged.exchange (false)) engine.setSeed (seed.load());

    if (work.getNumSamples() < n) work.setSize (2, n, false, false, true);
    if ((int) hiMain.size() < n * osFactor)
    {
        hiMain.resize ((size_t) (n * osFactor)); hiPoly.resize ((size_t) (n * osFactor)); hiExt.resize ((size_t) (n * osFactor));
    }

    // ingresso esterno in mono, portato alla frequenza del motore
    readParams();
    const bool useExt = numIn > 0 && ep.extVol > 0.0f;
    work.clear();
    if (useExt)
        for (int ch = 0; ch < numIn; ++ch)
            work.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);
    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
    juce::dsp::AudioBlock<float> hi;
    if (oversampler)
    {
        hi = oversampler->processSamplesUp (block);
        if (useExt) std::copy (hi.getChannelPointer (0), hi.getChannelPointer (0) + n * osFactor, hiExt.begin());
    }
    else if (useExt) std::copy (work.getReadPointer (0), work.getReadPointer (0) + n, hiExt.begin());

    engine.setParams (ep);
    int pos = 0;
    auto run = [&] (int to)
    {
        if (to > pos)
        {
            const int a = pos * osFactor, len = (to - pos) * osFactor;
            engine.process (hiMain.data() + a, hiPoly.data() + a, len, useExt ? hiExt.data() + a : nullptr);
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

    if (oversampler)
    {
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
    sleepingFlag.store (engine.isSleeping());
}

//==============================================================================
// Preset
bool DsProcessor::isGlobal (const juce::String& id)
{
    // impostazioni dello strumento, non del suono: un preset non le cambia
    for (auto* g : { pid::bendRange, pid::keyRange, pid::quality, pid::tune, pid::tolerances,
                     pid::polySplit, pid::pedal, pid::bend })
        if (id == g) return true;
    return false;
}

juce::ValueTree DsProcessor::soundState() const
{
    juce::ValueTree t ("DISSONA2PRESET");
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isGlobal (rp->paramID))
                t.setProperty (rp->paramID, rp->convertFrom0to1 (rp->getValue()), nullptr);
    return t;
}

void DsProcessor::applySoundState (const juce::ValueTree& t)
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isGlobal (rp->paramID) && t.hasProperty (rp->paramID))
            {
                const float v = rp->convertTo0to1 ((float) t.getProperty (rp->paramID));
                rp->beginChangeGesture(); rp->setValueNotifyingHost (v); rp->endChangeGesture();
            }
}

void DsProcessor::applyValues (const std::vector<std::pair<const char*, float>>& values)
{
    for (const auto& [id, plain] : values)
        if (auto* rp = apvts.getParameter (id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
            rp->endChangeGesture();
        }
}

void DsProcessor::markLoaded (const juce::String& name, int factory)
{
    presetName = name;
    factoryIndex = factory;
    loadedSnapshot.clear();
    for (auto* p : getParameters()) loadedSnapshot.push_back (p->getValue());
    abState = {};
    abSlot = 0;
}

bool DsProcessor::presetModified() const
{
    const auto& ps = getParameters();
    if ((int) loadedSnapshot.size() != ps.size()) return false;
    for (int i = 0; i < ps.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
            if (! isGlobal (rp->paramID) && std::abs (ps[i]->getValue() - loadedSnapshot[(size_t) i]) > 1.0e-4f)
                return true;
    return false;
}

void DsProcessor::loadFactory (int index)
{
    const auto& list = presets::factory();
    if (index < 0 || index >= (int) list.size()) return;
    applyValues (presets::neutral());
    applyValues (list[(size_t) index].values);
    userFile = juce::File();
    markLoaded (juce::String::fromUTF8 (list[(size_t) index].name), index);
}

const juce::String DsProcessor::getProgramName (int index)
{
    const auto& list = presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String::fromUTF8 (list[(size_t) index].name) : juce::String();
}

juce::File DsProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Collateral Effects").getChildFile ("DISSONA 2").getChildFile ("Preset");
}

juce::Array<juce::File> DsProcessor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.dissona2");
    files.sort();
    return files;
}

bool DsProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("DISSONA2PRESET")) return false;
    applyValues (presets::neutral());
    applySoundState (t);
    userFile = f;
    markLoaded (f.getFileNameWithoutExtension(), -1);
    return true;
}

bool DsProcessor::saveUserPreset (const juce::File& target)
{
    auto f = target.withFileExtension ("dissona2");
    f.getParentDirectory().createDirectory();
    auto t = soundState();
    t.setProperty ("name", f.getFileNameWithoutExtension(), nullptr);
    if (auto xml = t.createXml(); xml == nullptr || ! xml->writeTo (f)) return false;
    userFile = f;
    markLoaded (f.getFileNameWithoutExtension(), -1);
    return true;
}

void DsProcessor::stepPreset (int delta)
{
    // elenco unico: preset di fabbrica, poi preset utente
    const int nf = (int) presets::factory().size();
    const auto users = userPresets();
    const int total = nf + users.size();
    int cur = factoryIndex >= 0 ? factoryIndex : nf + juce::jmax (0, users.indexOf (userFile));
    cur = ((cur + delta) % total + total) % total;
    if (cur < nf) loadFactory (cur);
    else loadUserPreset (users[cur - nf]);
}

void DsProcessor::toggleAB()
{
    abState[(size_t) abSlot] = soundState();
    abSlot ^= 1;
    if (abState[(size_t) abSlot].isValid()) applySoundState (abState[(size_t) abSlot]);
    else abState[(size_t) abSlot] = soundState();   // la prima volta B parte uguale ad A
}

void DsProcessor::copyCurrentToOther()
{
    abState[(size_t) (abSlot ^ 1)] = soundState();
}

//==============================================================================
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
    state.setProperty ("seed", (int) seed.load(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("factoryIndex", factoryIndex, nullptr);
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
        if (state.hasProperty ("seed"))
        {
            seed.store ((uint32_t) (int) state.getProperty ("seed"));
            seedChanged.store (true);
        }
        const juce::String name = state.getProperty ("presetName", presetName).toString();
        const int fi = state.getProperty ("factoryIndex", -1);
        state.removeProperty ("seed", nullptr);
        state.removeProperty ("presetName", nullptr);
        state.removeProperty ("factoryIndex", nullptr);
        apvts.replaceState (state);
        // replaceState salta i parametri il cui valore "denormalizzato" non cambia (per esempio un
        // interruttore a 0,2 che vale già "spento"): li riallinea esattamente al valore salvato
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto child = state.getChildWithProperty ("id", rp->paramID);
                if (! child.isValid()) continue;
                const float target = rp->convertTo0to1 ((float) child.getProperty ("value"));
                if (std::abs (rp->getValue() - target) > 1.0e-6f) rp->setValueNotifyingHost (target);
            }
        markLoaded (name, fi);
    }
}

juce::AudioProcessorEditor* DsProcessor::createEditor() { return new DsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DsProcessor(); }
