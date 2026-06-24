#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthSound.h"
#include "SynthVoice.h"

FiSynthAudioProcessor::FiSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // APVTS tworzymy tutaj: (kto jest właścicielem, undo-manager=brak,
      // nazwa drzewa stanu, lista parametrów).
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new FiSynthVoice());

    synth.addSound (new FiSynthSound());

    for (int i = 0; i < kNumEnvelopes; ++i)
    {
        for (int p = 0; p < kMaxPlayheads; ++p)
        {
            envPlayheadTime[i][p].store (-1.0f);
            envPlayheadNote[p].store (-1);
        }
        // Wypełnij migawki startowe z domyślnych obwiedni.
        commitEnvelope (i);
    }
}

void FiSynthAudioProcessor::commitEnvelope (int idx)
{
    idx = juce::jlimit (0, kNumEnvelopes - 1, idx);
    {
        const juce::SpinLock::ScopedLockType sl (envLock);
        sharedSnapshots[idx] = envModels[idx].makeSnapshot();
    }
    envDirty.store (true);
}

void FiSynthAudioProcessor::convertEnvelopeTimes (double factor)
{
    if (factor <= 0.0 || std::abs (factor - 1.0) < 1.0e-9)
        return;

    for (int i = 0; i < kNumEnvelopes; ++i)
    {
        for (auto& p : envModels[i].points)
            p.time = (float) (p.time * factor);

        envModels[i].sortAndClamp();
        commitEnvelope (i);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
FiSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 },
        "Gain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.8f));

    // Synchronizacja obwiedni do tempa.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "envSync", 1 }, "Env Tempo Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "envGrid", 1 }, "Env Grid",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "envSnap", 1 }, "Env Snap To Grid", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "bpm", 1 }, "BPM (manual)",
        juce::NormalisableRange<float> { 20.0f, 300.0f, 0.01f }, 120.0f));

    // Źródło tempa: wł. = synchronizacja z DAW (BPM hosta), wył. = ręczne BPM.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "tempoSync", 1 }, "Tempo Sync (DAW)", true));

    // Routing modulacji: obwiednia jako ŹRÓDŁO -> 3 sloty (cel + głębokość).
    for (int slot = 0; slot < 3; ++slot)
    {
        juce::String prefix = "env" + juce::String (slot + 1);

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "Dest", 1 },
            "Env Dest " + juce::String (slot + 1),
            juce::StringArray { "None", "Filter Cutoff", "Pitch", "Osc Mix", "Resonance",
                                "Stretch 1", "Stretch 2", "Stretch 3" },
            0));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "Amt", 1 },
            "Env Amount " + juce::String (slot + 1),
            juce::NormalisableRange<float> { -1.0f, 1.0f, 0.01f },
            0.0f));
    }

    // 3 Oscylatory
    for (int o = 0; o < 3; ++o)
    {
        juce::String prefix = "osc" + juce::String (o + 1);

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "waveform", 1 },
            prefix + " Waveform",
            juce::StringArray { "Sine", "Square", "Triangle", "Sawtooth", "Quadratic", "Noise" },
            o == 0 ? 0 : 3));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "detune", 1 },
            prefix + " Detune",
            juce::NormalisableRange<float> { -48.0f, 48.0f, 1.0f },
            o == 0 ? 0.0f : (o == 1 ? 12.0f : -12.0f)));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "mix", 1 },
            prefix + " Mix",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
            0.333f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "stretch", 1 },
            prefix + " Stretch",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
            0.0f));
    }

    // Filter
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "filterCutoff", 1 },
        "Filter Cutoff",
        juce::NormalisableRange<float> { 20.0f, 20000.0f, 10.0f, 0.4f },
        5000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "filterResonance", 1 },
        "Filter Resonance",
        juce::NormalisableRange<float> { 0.1f, 10.0f, 0.1f },
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "filterType", 1 },
        "Filter Type",
        juce::StringArray { "Low-pass", "Band-pass", "High-pass" },
        0));

    // LFO
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoRate", 1 },
        "LFO Rate",
        juce::NormalisableRange<float> { 0.1f, 20.0f, 0.1f, 0.5f },
        1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoDepth", 1 },
        "LFO Depth",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.5f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lfoShape", 1 },
        "LFO Shape",
        juce::StringArray { "Sine", "Square", "Triangle" },
        0));

    // LFO sync do tempa: wł. = rate liczone z podziału nut (poniżej), wył. = Hz.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "lfoSync", 1 }, "LFO Tempo Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lfoRateDiv", 1 }, "LFO Rate (sync)",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T" }, 2));

    return { params.begin(), params.end() };
}

void FiSynthAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

#if FISYNTH_TEST_MODE
    // Krok sekwencji w próbkach (min. 1, żeby nie zapętlić się w processBlock).
    testStepSamples = juce::jmax (1, (int) (sampleRate * FISYNTH_TEST_STEP_MS / 1000.0));
    testSampleCounter = 0;
    testSeqIndex = 0;
    testCurrentNote = -1;
#endif
}

void FiSynthAudioProcessor::releaseResources() {}

bool FiSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void FiSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

#if FISYNTH_TEST_MODE
    // Tryb testowy: sami generujemy nuty na timerze próbkowym i dorzucamy je
    // do bufora MIDI, tak jakby przyszły z klawiatury. Co krok: noteOff
    // poprzedniej nuty + noteOn kolejnej z sekwencji.
    {
        const int numSamples = buffer.getNumSamples();
        int samplePos = 0;

        while (samplePos < numSamples)
        {
            const int samplesUntilStep = testStepSamples - testSampleCounter;
            const int advance = juce::jmin (samplesUntilStep, numSamples - samplePos);
            samplePos += advance;
            testSampleCounter += advance;

            if (testSampleCounter >= testStepSamples)
            {
                testSampleCounter = 0;
                const int offset = juce::jmin (samplePos, numSamples - 1);

                if (testCurrentNote >= 0)
                    midiMessages.addEvent (juce::MidiMessage::noteOff (1, testCurrentNote), offset);

                testCurrentNote = testSequence[(size_t) testSeqIndex];
                midiMessages.addEvent (
                    juce::MidiMessage::noteOn (1, testCurrentNote, (juce::uint8) 100), offset);

                testSeqIndex = (testSeqIndex + 1) % (int) testSequence.size();
            }
        }
    }
#endif

    // Jeśli GUI zmieniło którąkolwiek obwiednię, zabierz świeże migawki (bez
    // blokowania audio — gdy lock zajęty, spróbujemy w następnym bloku).
    if (envDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (envLock);
        if (tl.isLocked())
        {
            for (int i = 0; i < kNumEnvelopes; ++i)
                audioSnapshots[i] = sharedSnapshots[i];
            envDirty.store (false);
        }
    }

    // Tempo: gdy Tempo Sync wł., bierzemy BPM z hosta (DAW); gdy wył. (lub host
    // nie podaje tempa, np. standalone) — używamy ręcznego parametru BPM. Gdy
    // envSync wł., obwiednie liczone są w beatach, więc skalujemy je przez
    // sekundy-na-beat (timeScale).
    const bool tempoSync = apvts.getRawParameterValue ("tempoSync")->load() > 0.5f;

    double hostBpm = 0.0;
    if (tempoSync)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    hostBpm = *b;

    const float manualBpm = apvts.getRawParameterValue ("bpm")->load();
    const float bpm = hostBpm > 0.0 ? (float) hostBpm : manualBpm;
    currentBpm.store (bpm);

    const bool   envSync   = apvts.getRawParameterValue ("envSync")->load() > 0.5f;
    const double timeScale = envSync ? (60.0 / juce::jmax (1.0f, bpm)) : 1.0;

    // Routing modulacji obwiedni (3 sloty).
    int   envDest[3];
    float envAmt[3];
    for (int slot = 0; slot < 3; ++slot)
    {
        juce::String prefix = "env" + juce::String (slot + 1);
        envDest[slot] = static_cast<int> (*apvts.getRawParameterValue (prefix + "Dest"));
        envAmt[slot]  = apvts.getRawParameterValue (prefix + "Amt")->load();
    }

    // Odczyt parametrów.
    float filterCutoff = apvts.getRawParameterValue ("filterCutoff")->load();
    float filterResonance = apvts.getRawParameterValue ("filterResonance")->load();
    int filterType = static_cast<int> (*apvts.getRawParameterValue ("filterType"));

    // LFO rate: w trybie sync liczone z podziału nut pod bieżące BPM
    // (rate[Hz] = (BPM/60) / beaty-na-cykl); inaczej z ręcznego suwaka w Hz.
    const bool lfoSync = apvts.getRawParameterValue ("lfoSync")->load() > 0.5f;
    float lfoRate;
    if (lfoSync)
    {
        float lfoBeats = 1.0f;
        switch ((int) *apvts.getRawParameterValue ("lfoRateDiv"))
        {
            case 0: lfoBeats = 4.0f;        break;  // 1/1
            case 1: lfoBeats = 2.0f;        break;  // 1/2
            case 2: lfoBeats = 1.0f;        break;  // 1/4
            case 3: lfoBeats = 0.5f;        break;  // 1/8
            case 4: lfoBeats = 0.25f;       break;  // 1/16
            case 5: lfoBeats = 1.0f / 3.0f; break;  // 1/8T
            case 6: lfoBeats = 1.0f / 6.0f; break;  // 1/16T
            default: break;
        }
        lfoRate = (bpm / 60.0f) / juce::jmax (1.0e-4f, lfoBeats);
    }
    else
        lfoRate = apvts.getRawParameterValue ("lfoRate")->load();

    float lfoDepth = apvts.getRawParameterValue ("lfoDepth")->load();
    int lfoShape = static_cast<int> (*apvts.getRawParameterValue ("lfoShape"));

    // Synchronizuj na wszystkie głosy.
    for (int i = 0; i < numVoices; ++i)
    {
        if (auto* voice = dynamic_cast<FiSynthVoice*> (synth.getVoice (i)))
        {
            for (int e = 0; e < kNumEnvelopes; ++e)
                voice->setEnvelope (e, &audioSnapshots[e]);
            voice->setEnvTimeScale (timeScale);
            voice->setFilterParams (filterCutoff, filterResonance, filterType);
            voice->setLFOParams (lfoRate, lfoDepth, lfoShape);

            for (int slot = 0; slot < 3; ++slot)
                voice->setEnvMod (slot, envDest[slot], envAmt[slot]);

            for (int o = 0; o < 3; ++o)
            {
                juce::String prefix = "osc" + juce::String (o + 1);
                int waveform = static_cast<int> (*apvts.getRawParameterValue (prefix + "waveform"));
                float detune = apvts.getRawParameterValue (prefix + "detune")->load();
                float mix = apvts.getRawParameterValue (prefix + "mix")->load();
                float stretch = apvts.getRawParameterValue (prefix + "stretch")->load();
                voice->setOscillatorParams (o, waveform, detune, mix, stretch);
            }
        }
    }

    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Playhead: zbierz do kMaxPlayheads aktywnych głosów, każdy jako osobna kreska
    // (czas bezwzględny + poziom), osobno dla każdej obwiedni.
    {
        int slot = 0;
        for (int i = 0; i < numVoices && slot < kMaxPlayheads; ++i)
        {
            if (auto* voice = dynamic_cast<FiSynthVoice*> (synth.getVoice (i)))
            {
                if (voice->isEnvActive())
                {
                    for (int e = 0; e < kNumEnvelopes; ++e)
                        envPlayheadTime[e][slot].store (voice->getEnvTime (e));

                    envPlayheadNote[slot].store (voice->getCurrentlyPlayingNote());
                    ++slot;
                }
            }
        }

        // Wyczyść pozostałe sloty (mniej głosów niż kMaxPlayheads).
        for (; slot < kMaxPlayheads; ++slot)
        {
            for (int e = 0; e < kNumEnvelopes; ++e)
                envPlayheadTime[e][slot].store (-1.0f);
            envPlayheadNote[slot].store (-1);
        }
    }

    const float gain = apvts.getRawParameterValue ("gain")->load();
    buffer.applyGainRamp (0, buffer.getNumSamples(), previousGain, gain);
    previousGain = gain;
}

juce::AudioProcessorEditor* FiSynthAudioProcessor::createEditor()
{
    return new FiSynthAudioProcessorEditor (*this);
}

// Serializuje pełny stan (parametry + obwiednie) do XML. Współdzielone przez
// stan DAW i pliki presetów.
std::unique_ptr<juce::XmlElement> FiSynthAudioProcessor::stateToXml()
{
    // Wpisz aktualne punkty każdej obwiedni jako osobne dziecko drzewa stanu.
    for (int i = 0; i < kNumEnvelopes; ++i)
    {
        auto envTree = apvts.state.getOrCreateChildWithName (
            juce::Identifier ("ENVELOPE" + juce::String (i)), nullptr);
        envModels[i].toValueTree (envTree);
    }

    return apvts.copyState().createXml();
}

// Odtwarza pełny stan z XML (parametry + obwiednie). Współdzielone przez stan
// DAW i pliki presetów.
void FiSynthAudioProcessor::applyStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (xml));

    // Odtwórz punkty każdej obwiedni i wepchnij migawki do audio.
    // Fallback: stary stan z jedną obwiednią ("ENVELOPE") -> obwiednia 0.
    for (int i = 0; i < kNumEnvelopes; ++i)
    {
        auto child = apvts.state.getChildWithName (
            juce::Identifier ("ENVELOPE" + juce::String (i)));
        if (i == 0 && ! child.isValid())
            child = apvts.state.getChildWithName ("ENVELOPE");

        envModels[i].fromValueTree (child);
        commitEnvelope (i);
    }
}

void FiSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateToXml())
        copyXmlToBinary (*xml, destData);
}

void FiSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        applyStateXml (*xml);
}

// === Presety ===

juce::File FiSynthAudioProcessor::getPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("FiSynth")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

bool FiSynthAudioProcessor::savePreset (const juce::String& name)
{
    const auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty())
        return false;

    auto file = getPresetDirectory().getChildFile (safe + ".fsynth");
    if (auto xml = stateToXml(); xml != nullptr && xml->writeTo (file))
    {
        currentPresetName = safe;
        return true;
    }
    return false;
}

bool FiSynthAudioProcessor::loadPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        applyStateXml (*xml);
        currentPresetName = file.getFileNameWithoutExtension();
        return true;
    }
    return false;
}

juce::StringArray FiSynthAudioProcessor::getPresetList() const
{
    juce::StringArray names;
    for (auto& f : getPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.fsynth"))
        names.add (f.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

void FiSynthAudioProcessor::resetToInit()
{
    // Ustawia parametr po wartości "fizycznej" (plain), konwertując na 0..1.
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };
    // Ustawia parametr wprost po wartości znormalizowanej 0..1 (np. "połowa").
    auto setNorm = [this] (const juce::String& id, float n)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, n));
    };

    set ("gain", 0.5f);

    // Oscylatory: wszystkie sinusy, bez detune/stretch; słyszalny tylko 1. (mix=0.5).
    for (int o = 0; o < 3; ++o)
    {
        const juce::String pre = "osc" + juce::String (o + 1);
        set (pre + "waveform", 0.0f);              // Sine
        set (pre + "detune",   0.0f);
        set (pre + "mix",      o == 0 ? 0.5f : 0.0f);
        set (pre + "stretch",  0.0f);
    }

    // Modulacje wyzerowane.
    for (int s = 0; s < 3; ++s)
    {
        const juce::String pre = "env" + juce::String (s + 1);
        set (pre + "Dest", 0.0f);                  // None
        set (pre + "Amt",  0.0f);
    }

    // Filtr: cutoff w połowie, brak rezonansu, low-pass.
    setNorm ("filterCutoff", 0.5f);
    set     ("filterResonance", 0.1f);             // minimum (neutralnie)
    set     ("filterType", 0.0f);                  // Low-pass

    // LFO bezczynne.
    set ("lfoDepth", 0.0f);
    set ("lfoShape", 0.0f);                        // Sine
    set ("lfoRate",  1.0f);
    set ("lfoSync",  0.0f);

    set ("envSync", 0.0f);                         // bez sync do tempa

    // Obwiednie do domyślnego ADSR.
    for (int i = 0; i < kNumEnvelopes; ++i)
    {
        getEnvelopeModel (i).setDefault();
        commitEnvelope (i);
    }

    currentPresetName.clear();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FiSynthAudioProcessor();
}
