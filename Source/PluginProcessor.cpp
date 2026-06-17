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

    // ADSR (4 suwaki)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 },
        "Attack",
        juce::NormalisableRange<float> { 0.001f, 2.0f, 0.001f, 0.5f },
        0.05f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 },
        "Decay",
        juce::NormalisableRange<float> { 0.001f, 2.0f, 0.001f, 0.5f },
        0.1f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sustain", 1 },
        "Sustain",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 },
        "Release",
        juce::NormalisableRange<float> { 0.001f, 2.0f, 0.001f, 0.5f },
        0.4f));

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

    // Odczyt parametrów.
    float attackValue = apvts.getRawParameterValue ("attack")->load();
    float decayValue = apvts.getRawParameterValue ("decay")->load();
    float sustainValue = apvts.getRawParameterValue ("sustain")->load();
    float releaseValue = apvts.getRawParameterValue ("release")->load();

    float filterCutoff = apvts.getRawParameterValue ("filterCutoff")->load();
    float filterResonance = apvts.getRawParameterValue ("filterResonance")->load();
    int filterType = static_cast<int> (*apvts.getRawParameterValue ("filterType"));

    float lfoRate = apvts.getRawParameterValue ("lfoRate")->load();
    float lfoDepth = apvts.getRawParameterValue ("lfoDepth")->load();
    int lfoShape = static_cast<int> (*apvts.getRawParameterValue ("lfoShape"));

    // Synchronizuj na wszystkie głosy.
    for (int i = 0; i < numVoices; ++i)
    {
        if (auto* voice = dynamic_cast<FiSynthVoice*> (synth.getVoice (i)))
        {
            voice->setADSR (attackValue, decayValue, sustainValue, releaseValue);
            voice->setFilterParams (filterCutoff, filterResonance, filterType);
            voice->setLFOParams (lfoRate, lfoDepth, lfoShape);

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

    const float gain = apvts.getRawParameterValue ("gain")->load();
    buffer.applyGainRamp (0, buffer.getNumSamples(), previousGain, gain);
    previousGain = gain;
}

juce::AudioProcessorEditor* FiSynthAudioProcessor::createEditor()
{
    return new FiSynthAudioProcessorEditor (*this);
}

void FiSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Zapis całego drzewa parametrów do bloku bajtów (preset / stan w DAW).
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FiSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Odtworzenie stanu z bajtów (otwarcie projektu / wczytanie presetu).
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FiSynthAudioProcessor();
}
