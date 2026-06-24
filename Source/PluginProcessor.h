#pragma once

#include <JuceHeader.h>
#include "EnvelopeModel.h"

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

    // === Presety (zapis/wczytywanie pełnego stanu do plików .fsynth) ===
    // Katalog presetów (tworzony przy pierwszym użyciu).
    static juce::File getPresetDirectory();

    // Zapisuje bieżący stan pod podaną nazwą (nazwa pliku jest sanityzowana).
    // Zwraca true gdy zapis się powiódł.
    bool savePreset (const juce::String& name);

    // Wczytuje preset z pliku. Zwraca true gdy się powiodło.
    bool loadPreset (const juce::File& file);

    // Lista nazw dostępnych presetów (posortowana, bez rozszerzenia).
    juce::StringArray getPresetList() const;

    // Reset do neutralnego patcha "init": wszystkie oscylatory = sinus, modulacje
    // i LFO wyzerowane, poziomy w połowie, obwiednie do domyślnego ADSR.
    void resetToInit();

    // Nazwa aktualnie wczytanego presetu (do wyświetlenia w GUI). Pusta = brak.
    // Dotykana tylko z wątku komunikatów (GUI), więc bez atomika.
    juce::String currentPresetName;

    // Publiczne, bo edytor (GUI) musi się dobrać do parametrów.
    juce::AudioProcessorValueTreeState apvts;

    // === Obwiednie wielopunktowe (kNumEnvelopes sztuk) ===
    // Modele edytowalne (wątek GUI). Edytor zmienia punkty wybranej obwiedni,
    // po czym woła commitEnvelope(idx), które wpycha jej migawkę do audio.
    EnvelopeModel& getEnvelopeModel (int idx) noexcept
    {
        return envModels[(size_t) juce::jlimit (0, kNumEnvelopes - 1, idx)];
    }
    void commitEnvelope (int idx);

    // Przeskalowuje czasy punktów WSZYSTKICH obwiedni przez podany współczynnik
    // (np. przy przełączaniu sync: sekundy<->beaty), zachowując realną długość.
    void convertEnvelopeTimes (double factor);

    // Playhead — do kMaxPlayheads jednocześnie grających głosów, osobno dla każdej
    // obwiedni. Time = czas bezwzględny w jednostkach obwiedni (mapowany przez
    // timeToX w GUI), -1 = slot pusty / obwiednia nieaktywna. Poziom (oś Y) GUI
    // liczy z krzywej w tym czasie, więc kropka zawsze leży na obwiedni.
    // Czytane przez timer GUI, pisane przez audio.
    static constexpr int kMaxPlayheads = 6;
    std::atomic<float> envPlayheadTime [kNumEnvelopes][kMaxPlayheads];

    // Numer nuty MIDI grany w danym slocie (wspólny dla wszystkich obwiedni tego
    // głosu) — do legendy w GUI. -1 = slot pusty.
    std::atomic<int>   envPlayheadNote [kMaxPlayheads];

    // === Synchronizacja obwiedni do tempa ===
    // Gdy sync włączony, czasy punktów obwiedni traktujemy jako BEATY (ćwiartki),
    // a wątek audio skaluje je przez sekundy-na-beat z bieżącego BPM (z DAW,
    // a w razie braku — z ręcznego parametru). Dzięki temu długość ADSR podąża
    // za tempem. Editor (GUI) czyta poniższe, żeby rysować siatkę i snapować.
    bool  isEnvSync() const noexcept
    {
        return apvts.getRawParameterValue ("envSync")->load() > 0.5f;
    }
    bool  isEnvSnap() const noexcept
    {
        return apvts.getRawParameterValue ("envSnap")->load() > 0.5f;
    }
    // Krok siatki w beatach (ćwiartka = 1.0).
    float gridDivisionBeats() const noexcept
    {
        switch ((int) *apvts.getRawParameterValue ("envGrid"))
        {
            case 0:  return 1.0f;          // 1/4
            case 1:  return 0.5f;          // 1/8
            case 2:  return 0.25f;         // 1/16
            case 3:  return 0.125f;        // 1/32
            case 4:  return 1.0f / 3.0f;   // 1/8T
            case 5:  return 1.0f / 6.0f;   // 1/16T
            default: return 0.25f;
        }
    }

    // Aktualne efektywne BPM (z DAW lub ręczne). Pisane przez audio, czytane GUI.
    std::atomic<float> currentBpm { 120.0f };

private:
    // Buduje listę wszystkich parametrów pluginu. static, bo wołane
    // w liście inicjalizacyjnej konstruktora, zanim obiekt w pełni istnieje.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Serializacja pełnego stanu (parametry + obwiednie) — współdzielona przez
    // stan DAW (get/setStateInformation) i pliki presetów.
    std::unique_ptr<juce::XmlElement> stateToXml();
    void applyStateXml (const juce::XmlElement& xml);

    juce::Synthesiser synth;
    static constexpr int numVoices = 8;

    // Modele edytowalne + dwie tablice migawek POD: 'shared' pisana przez GUI
    // pod spinlockiem, 'audio' to prywatna kopia wątku audio (bez alokacji).
    EnvelopeModel envModels[kNumEnvelopes];
    EnvSnapshot   sharedSnapshots[kNumEnvelopes];
    EnvSnapshot   audioSnapshots[kNumEnvelopes];
    juce::SpinLock     envLock;
    std::atomic<bool>  envDirty { true };

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
