#pragma once

#include <JuceHeader.h>
#include <climits>
#include "EnvelopeModel.h"

class FiSynthVoice;

// === Tryb testowy: auto-trigger nut bez klawiatury MIDI ===
// 0 = normalny tryb — syntezator gra to, co przyjdzie z MIDI (klawiatura/DAW).
// 1 = tryb testowy — plugin sam, na timerze próbkowym, gra w kółko sekwencję
//     (arpeggio), żeby można było usłyszeć barwę bez kontrolera MIDI.
// Przełączasz wartość i rekompilujesz.
#define FISYNTH_TEST_MODE      0
#define FISYNTH_TEST_STEP_MS   400   // długość jednego kroku sekwencji w ms

class FiSynthAudioProcessor : public juce::AudioProcessor,
                              private juce::Timer
{
public:
    FiSynthAudioProcessor();
    // Jawny destruktor: stopTimer() PRZED destrukcją składowych. Domyślny
    // zatrzymałby timer dopiero w ~Timer (bazy niszczą się po składowych),
    // a tick w tym oknie czytałby martwe ccFifo/ccMap.
    ~FiSynthAudioProcessor() override;

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
    // Ogon zależny od patcha (delay z feedbackiem i pogłos dzwonią długo) —
    // liczony z bieżących parametrów, żeby bounce/freeze nie ucinał ogona.
    double getTailLengthSeconds()         const override;

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

    // Stan klawiatury ekranowej: GUI wciska nuty myszą, processBlock wstrzykuje
    // je do strumienia MIDI (processNextMidiBuffer), a eventy z hosta aktualizują
    // stan klawiszy — klawiatura pokazuje więc też to, co gra DAW.
    juce::MidiKeyboardState keyboardState;

    // === Obwiednie wielopunktowe (kNumEnvelopes sztuk) ===
    // Modele edytowalne (wątek GUI). Edytor zmienia punkty wybranej obwiedni,
    // po czym woła commitEnvelope(idx), które wpycha jej migawkę do audio.
    EnvelopeModel& getEnvelopeModel (int idx) noexcept
    {
        return envModels[(size_t) juce::jlimit (0, kNumEnvelopes - 1, idx)];
    }
    void commitEnvelope (int idx);

    // Rośnie przy każdym commitEnvelope — GUI unieważnia nim cache tła edytora
    // obwiedni. Dotykany tylko z wątku komunikatów; atomik dla porządku.
    std::atomic<int> envEditCount { 0 };

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
        return par.envSync->load() > 0.5f;
    }
    bool  isEnvSnap() const noexcept
    {
        return par.envSnap->load() > 0.5f;
    }
    // Krok siatki w beatach (ćwiartka = 1.0) — ze wspólnej tabeli divisions.
    float gridDivisionBeats() const noexcept
    {
        return divisionToBeats ((int) par.envGrid->load());
    }

    // Aktualne efektywne BPM (z DAW lub ręczne). Pisane przez audio, czytane GUI.
    std::atomic<float> currentBpm { 120.0f };

    // === Gate Fibonacciego ===
    // Pattern to słowo Fibonacciego (podstawienia S→SL, L→S); S = krok otwarty.
    // Generacja wybiera długość 5/8/13/21/34. Flipy użytkownika (klik w kropkę
    // pierścienia) trzymane jako maska bitowa — atomik wystarcza za cały lock:
    // GUI robi fetch_xor, audio czyta. Zapisywane w stanie/presetach.
    struct FibWord { juce::uint64 bits; int len; };
    static FibWord fibonacciWord (int genIdx);           // genIdx 0..4

    std::atomic<juce::uint64> gateFlips { 0 };
    std::atomic<int>          gateStep  { -1 };          // bieżący krok (playhead GUI), -1 = gate wył.

    // === Wspólna tabela podziałów rytmicznych (siatka obwiedni + gate) ===
    // Jedno źródło prawdy dla etykiet, list Choice i przeliczeń na beaty —
    // indeksy muszą być spójne wszędzie, więc nigdzie nie ma drugiej kopii.
    struct Division { const char* label; float beats; };
    static constexpr Division divisions[6] = {
        { "1/4", 1.0f }, { "1/8", 0.5f }, { "1/16", 0.25f },
        { "1/32", 0.125f }, { "1/8T", 1.0f / 3.0f }, { "1/16T", 1.0f / 6.0f },
    };

    static juce::StringArray divisionLabels();

    static float divisionToBeats (int choiceIdx) noexcept
    {
        return divisions[(size_t) juce::jlimit (0, 5, choiceIdx)].beats;
    }

    // Pierwsza aktualnie grająca nuta (fundament dla wizualizerów); A2 gdy cisza.
    int firstActiveNote (int fallback = 45) const noexcept
    {
        for (int i = 0; i < kMaxPlayheads; ++i)
        {
            const int n = envPlayheadNote[i].load();
            if (n >= 0)
                return n;
        }
        return fallback;
    }

    // === FIFO próbek dla analizatora widma (audio pisze, GUI czyta) ===
    // Zwraca liczbę faktycznie przeczytanych próbek (mono suma po gainie).
    int readAnalyzerSamples (float* dest, int maxNum);

    // === MIDI Learn (CC -> parametr) ===
    // GUI (prawy klik na gałce/spirali) uzbraja Learn; pierwszy CC, który
    // przyjdzie, przejmuje przypisanie (1:1 — stare przypisania parametru
    // i tego CC znikają). Całość żyje na wątku komunikatów: audio tylko
    // zrzuca eventy CC do FIFO, a timerCallback mapuje je na parametry
    // (setValueNotifyingHost wolno wołać wyłącznie z wątku komunikatów).
    void startMidiLearn (const juce::String& paramID);
    void cancelMidiLearn();
    bool isMidiLearnArmed (const juce::String& paramID) const;
    int  ccForParam (const juce::String& paramID) const;   // -1 = brak
    void clearCcMapping (const juce::String& paramID);

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

    // Głosy w typie konkretnym — bez dynamic_cast w każdym bloku audio.
    // Właścicielem obiektów jest synth; wskaźniki żyją tak długo jak procesor.
    std::vector<FiSynthVoice*> fiVoices;

    // Surowe wskaźniki parametrów. getRawParameterValue robi lookup po stringu
    // (a składanie nazw konkatenacją alokuje na heapie), więc wątek audio czyta
    // wyłącznie przez ten cache, wypełniany raz w konstruktorze.
    struct ParamPtrs
    {
        std::atomic<float>* gain;
        std::atomic<float>* tempoSync;
        std::atomic<float>* bpm;
        std::atomic<float>* envSync;
        std::atomic<float>* envSnap;
        std::atomic<float>* envGrid;
        std::atomic<float>* filterCutoff;
        std::atomic<float>* filterResonance;
        std::atomic<float>* filterType;
        std::atomic<float>* lfoRate;
        std::atomic<float>* lfoDepth;
        std::atomic<float>* lfoShape;
        std::atomic<float>* lfoSync;
        std::atomic<float>* lfoRateDiv;
        std::atomic<float>* envDest[3];
        std::atomic<float>* envAmt[3];
        std::atomic<float>* gateOn;
        std::atomic<float>* gateDepth;
        std::atomic<float>* gateDiv;
        std::atomic<float>* gateGen;
        std::atomic<float>* spread;

        // Złoty silnik + drift LFO + kwantyzacja pitch-modu.
        std::atomic<float>* ringMix;
        std::atomic<float>* fmAmt;
        std::atomic<float>* subLevel;
        std::atomic<float>* unison;
        std::atomic<float>* lfoDrift;
        std::atomic<float>* pitchQuant;

        // Arpeggiator Fibonacciego.
        std::atomic<float>* arpOn;
        std::atomic<float>* arpDiv;
        std::atomic<float>* arpLen;
        std::atomic<float>* arpMode;
        std::atomic<float>* arpWord;
        std::atomic<float>* arpVel;

        // Golden Delay (multi-tap w proporcjach φ).
        std::atomic<float>* dlyOn;
        std::atomic<float>* dlySync;
        std::atomic<float>* dlyTime;
        std::atomic<float>* dlyDiv;
        std::atomic<float>* dlyFeedback;
        std::atomic<float>* dlyMix;

        // Efektor (distortion / saturation / waveshaper / reverb).
        std::atomic<float>* fxOn;
        std::atomic<float>* fxDist;
        std::atomic<float>* fxSat;
        std::atomic<float>* fxShape;
        std::atomic<float>* fxRevSize;
        std::atomic<float>* fxRevMix;

        struct Osc
        {
            std::atomic<float>* waveform;
            std::atomic<float>* detune;
            std::atomic<float>* mix;
            std::atomic<float>* stretch;
            std::atomic<float>* stretchMode;
            std::atomic<float>* goldInt;
            std::atomic<float>* tilt;
        } osc[3];
    } par;

    // Modele edytowalne + dwie tablice migawek POD: 'shared' pisana przez GUI
    // pod spinlockiem, 'audio' to prywatna kopia wątku audio (bez alokacji).
    EnvelopeModel envModels[kNumEnvelopes];
    EnvSnapshot   sharedSnapshots[kNumEnvelopes];
    EnvSnapshot   audioSnapshots[kNumEnvelopes];
    juce::SpinLock     envLock;
    std::atomic<bool>  envDirty { true };

    // Pamiętamy poprzednią głośność, żeby robić płynny ramp (bez trzasków).
    float previousGain { 1.0f };

    // === Wspólny zegar beatowy (gate + arp) ===
    // JEDNA pozycja w beatach: ppq hosta gdy transport gra, inaczej wolnobieżna
    // — naliczana ZAWSZE (nie tylko gdy dany moduł włączony), więc patterny
    // gate'a i arpa nigdy nie dryfują względem siebie w standalone.
    // processBlock liczy pozycję startu bloku i beats-per-sample raz i podaje
    // je obu konsumentom.
    double beatPos { 0.0 };

    // Gate: wygładzona wartość bramki (~1.5 ms one-pole, bez trzasków).
    float  gateEnv { 1.0f };
    void   applyFibGate (juce::AudioBuffer<float>&, double blockStartBeat,
                         double beatsPerSample);

    // === Arpeggiator Fibonacciego ===
    // Przy włączonym arp eventy nut z MIDI tylko aktualizują zbiór trzymanych
    // klawiszy; grają wyłącznie nuty generowane krokami zegara (jak gate).
    // Melodia: root + interwały Fibonacciego w półtonach (0,1,2,3,5,8,13,21;
    // dalsze wyrazy składane mod 24, żeby nie uciec z rejestru).
    bool   arpHeld[128] {};
    int    arpLastStep { INT_MIN };
    int    arpNoteSounding { -1 };
    bool   arpWasOn { false };
    // Bufor roboczy filtra MIDI — pole klasy z ensureSize w prepareToPlay,
    // żeby processArp nie alokował na wątku audio (po swapWith oba bufory
    // recyklingują swoją pamięć między blokami).
    juce::MidiBuffer arpFiltered;

    // RNG trybu Random — własny egzemplarz (getSystemRandom nie jest audio-safe).
    juce::Random arpRng;
    void   processArp (juce::MidiBuffer&, int numSamples, double blockStartBeat,
                       double beatsPerSample);

    // === Golden Delay ===
    // Multi-tap: 4 tapy w czasach t/φ³..t (odstępy złote — niewspółmierne, więc
    // bez okresowego "fluteru" grzebieniowego), sprzężenie na krzyż (ping-pong).
    juce::AudioBuffer<float> delayBuffer;
    int   delayWritePos { 0 };
    float delaySmoothSamples { -1.0f };   // wygładzony czas (bez zgrzytu przy zmianie)
    bool  dlyWasOn { false };
    void  applyGoldenDelay (juce::AudioBuffer<float>&, float bpm);

    // === Efektor ===
    // Drive (dist → sat → fold) siedzi PRZED delayem (echa niosą już przester,
    // a feedback nie przesterowuje się kumulacyjnie); pogłos za delayem, na
    // końcu łańcucha (ogona nie tnie gate ani nie klipuje drive).
    void applyFxDrive (juce::AudioBuffer<float>&);
    void applyFxReverb (juce::AudioBuffer<float>&);
    juce::Reverb fxReverb;

    // Amounty drive'u wygładzane one-pole (~5 ms) per próbka — włącz/wyłącz
    // i automacja (też z MIDI Learn) bez skokowej podmiany kształtu fali.
    float fxDistEnv { 0.0f }, fxSatEnv { 0.0f }, fxShapeEnv { 0.0f };

    // Po wyłączeniu pogłosu krótki dogrywany ogon (wewnętrzny smoothing gainów
    // Reverbu ściąga wet do zera) — bez cięcia ogona między blokami; dopiero
    // po nim reset i pełny bypass.
    int fxRevTailSamples { 0 };

    // Analizator widma: lock-free FIFO mono sumy wyjścia.
    juce::AbstractFifo analyzerFifo { 1 << 14 };
    std::vector<float> analyzerBuffer = std::vector<float> (1 << 14, 0.0f);
    void pushAnalyzerSamples (const juce::AudioBuffer<float>&);

    // === MIDI Learn: FIFO eventów CC (audio pisze, timer czyta) ===
    // ccMap i learnParam chronione spinlockiem: spec VST3 każe wołać
    // set/getStateInformation z wątku UI, ale release-build hosta tego nie
    // egzekwuje (JUCE ma tam tylko debugowy assert) — lock kosztuje grosze,
    // a zamyka wyścig timer↔host. Audio locka NIE bierze: tylko pisze FIFO
    // i czyta atomową flagę ccActive (fałsz = mapa pusta i Learn nieuzbrojony,
    // pętla skanu CC w processBlock w ogóle nie rusza). Wskaźniki parametrów
    // żyją tak długo jak procesor. Przepełnione FIFO gubi nadmiar — kręcenie
    // gałką wysyła dziesiątki CC, strata paru jest niesłyszalna.
    void timerCallback() override;
    void updateCcActive() noexcept;          // wołane pod ccMapLock
    struct CcEvent { int num; float val; };
    static constexpr int kCcFifoSize = 512;
    juce::AbstractFifo ccFifo { kCcFifoSize };
    CcEvent ccEvents[kCcFifoSize] {};
    mutable juce::SpinLock ccMapLock;    // mutable: ccForParam/isMidiLearnArmed są const
    std::atomic<bool> ccActive { false };
    juce::RangedAudioParameter* ccMap[128] {};
    juce::RangedAudioParameter* learnParam { nullptr };

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
