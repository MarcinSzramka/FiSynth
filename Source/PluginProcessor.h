#pragma once

#include <JuceHeader.h>

// === Tryb testowy: auto-trigger nut bez klawiatury MIDI ===
// 0 = normalny tryb — syntezator gra to, co przyjdzie z MIDI (klawiatura/DAW).
// 1 = tryb testowy — plugin sam, na timerze próbkowym, gra w kółko sekwencję
//     (arpeggio), żeby można było usłyszeć barwę bez kontrolera MIDI.
// Przełączasz wartość i rekompilujesz.
#define FISYNTH_TEST_MODE      1
#define FISYNTH_TEST_STEP_MS   400   // długość jednego kroku sekwencji w ms

class FiSynthAudioProcessor : public juce::AudioProcessor
{
public:
    FiSynthAudioProcessor();
    ~FiSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool   acceptsMidi()                  const override   { return true; }
    bool   producesMidi()                 const override   { return false; }
    bool   isMidiEffect()                 const override   { return false; }
    double getTailLengthSeconds()         const override   { return 0.0; }

    int                getNumPrograms()                    override { return 1; }
    int                getCurrentProgram()                 override { return 0; }
    void               setCurrentProgram (int)             override {}
    const juce::String getProgramName (int)                override { return {}; }
    void               changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Publiczne, bo edytor (GUI) musi się dobrać do parametrów.
    juce::AudioProcessorValueTreeState apvts;

private:
    // Buduje listę wszystkich parametrów pluginu. static, bo wołane
    // w liście inicjalizacyjnej konstruktora, zanim obiekt w pełni istnieje.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::Synthesiser synth;
    static constexpr int numVoices = 8;

    // Pamiętamy poprzednią głośność, żeby robić płynny ramp (bez trzasków).
    float previousGain { 1.0f };

#if FISYNTH_TEST_MODE
    // Sekwencja grana w trybie testowym (numery nut MIDI). Domyślnie
    // arpeggio C-dur w górę i z powrotem: C E G C G E.
    std::vector<int> testSequence { 60, 64, 67, 72, 67, 64 };
    int testStepSamples   { 0 };   // długość kroku w próbkach (z prepareToPlay)
    int testSampleCounter { 0 };   // ile próbek upłynęło w bieżącym kroku
    int testSeqIndex      { 0 };   // następna nuta do zagrania
    int testCurrentNote   { -1 };  // aktualnie brzmiąca nuta (-1 = cisza)
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessor)
};
