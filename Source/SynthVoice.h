#pragma once

#include <JuceHeader.h>
#include "SynthSound.h"
#include "EnvelopeModel.h"
#include "PartialTables.h"
#include "ModMath.h"

// === Stanowy odtwarzacz wielopunktowej obwiedni (per-głos) ===
//
// Przechodzi przez punkty migawki: faza PreSustain (attack/decay) -> Sustain
// (trzymanie na punkcie sustain dopóki klawisz wciśnięty) -> Release (reszta
// punktów po puszczeniu klawisza, startując od aktualnej wartości, więc bez
// trzasków nawet przy puszczeniu w trakcie attacku).
struct EnvPlayer
{
    enum class Stage { Idle, PreSustain, Sustain, Release };

    void setSampleRate (double s) noexcept     { sr = (s > 0.0 ? s : sr); }
    void setSnapshot (const EnvSnapshot* s) noexcept { snap = s; }

    // Skala czasu: ile sekund trwa jedna jednostka czasu obwiedni. 1.0 = czasy
    // punktów w sekundach (tryb wolny). W trybie sync = sekundy-na-beat, więc
    // czasy punktów traktowane są jako beaty i długość obwiedni podąża za tempem.
    void setTimeScale (double s) noexcept { timeScale = (s > 1.0e-9 ? s : 1.0); }

    bool  isActive() const noexcept { return stage != Stage::Idle; }
    float getValue() const noexcept { return value; }

    void noteOn() noexcept
    {
        timePos      = 0.0;
        releaseClock = 0.0;
        value        = 0.0f;
        stage        = Stage::PreSustain;

        // sustain na pierwszym punkcie => od razu trzymamy.
        if (snap != nullptr && snap->numPoints > 0 && snap->sustainIndex <= 0)
            stage = Stage::Sustain;
    }

    void noteOff() noexcept
    {
        if (stage == Stage::Idle)
            return;

        releaseStartValue = value;
        releaseClock      = 0.0;
        stage             = Stage::Release;
    }

    void reset() noexcept
    {
        stage        = Stage::Idle;
        value        = 0.0f;
        timePos      = 0.0;
        releaseClock = 0.0;
    }

    float nextSample() noexcept
    {
        if (snap == nullptr || snap->numPoints == 0)
        {
            value = 0.0f;
            return value;
        }

        const auto&  s  = *snap;
        // Krok w jednostkach czasu obwiedni na próbkę. W trybie sync timeScale =
        // sekundy-na-beat, więc czasy punktów (beaty) skalują się z tempem.
        const double dt = (1.0 / sr) / timeScale;

        switch (stage)
        {
            case Stage::Idle:
                value = 0.0f;
                break;

            case Stage::PreSustain:
                value = fiEnvEvalRange (s, 0, s.sustainIndex, (float) timePos);
                timePos += dt;
                if (timePos >= s.times[s.sustainIndex])
                {
                    timePos = s.times[s.sustainIndex];
                    stage   = Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                value = s.levels[s.sustainIndex];
                break;

            case Stage::Release:
            {
                const float relLen = s.times[s.numPoints - 1] - s.times[s.sustainIndex];

                // Sustain na ostatnim punkcie => krzywa nie ma fazy release.
                // Krótki liniowy fade (~5 ms realnego czasu) zamiast cięcia do zera.
                if (relLen <= 0.0f)
                {
                    const double fadeUnits = 0.005 / timeScale;
                    const double f = releaseClock / fadeUnits;
                    value = releaseStartValue * (float) juce::jmax (0.0, 1.0 - f);
                    releaseClock += dt;
                    if (f >= 1.0)
                    {
                        stage = Stage::Idle;
                        value = 0.0f;
                    }
                    break;
                }

                value = evalRelease (s, (float) releaseClock);
                releaseClock += dt;
                if (releaseClock >= relLen)
                {
                    stage = Stage::Idle;
                    value = 0.0f;
                }
                break;
            }
        }

        return value;
    }

    // Czas BEZWZGLĘDNY (w jednostkach obwiedni: sekundy w trybie wolnym, beaty w
    // sync) bieżącej pozycji odtwarzania — dla playheada w GUI, mapowanego tą samą
    // funkcją timeToX() co krzywa, więc kropka leży dokładnie na krzywej.
    // Zwraca -1 gdy ta obwiednia jest nieaktywna (brak playheada).
    float absTime() const noexcept
    {
        if (snap == nullptr || snap->numPoints == 0)
            return -1.0f;

        const auto& s = *snap;
        switch (stage)
        {
            case Stage::PreSustain: return (float) timePos;
            case Stage::Sustain:    return s.times[s.sustainIndex];
            // Naturalne tempo: playhead idzie 1:1 z osią czasu krzywej (stała
            // prędkość, ta sama dla wszystkich głosów). Przy puszczeniu w trakcie
            // ataku przeskakuje na początek fazy release — ale bez przyspieszania.
            case Stage::Release:    return s.times[s.sustainIndex] + (float) releaseClock;
            case Stage::Idle:
            default:                return -1.0f;
        }
    }

private:
    // Release: od releaseStartValue, przez punkty (sustainIndex+1 .. koniec),
    // re-czasowane tak, że zaczynają się od 0 w momencie puszczenia klawisza.
    float evalRelease (const EnvSnapshot& s, float rt) const noexcept
    {
        const int sIdx = s.sustainIndex;
        const int last = s.numPoints - 1;

        if (sIdx >= last)        return 0.0f;   // brak fazy release
        if (rt <= 0.0f)          return releaseStartValue;

        const float base  = s.times[sIdx];
        float       prevT = 0.0f;
        float       prevL = releaseStartValue;

        for (int i = sIdx + 1; i <= last; ++i)
        {
            const float segT = s.times[i] - base;
            if (rt <= segT)
            {
                const float span = juce::jmax (1.0e-6f, segT - prevT);
                const float f    = (rt - prevT) / span;
                return prevL + (s.levels[i] - prevL) * fiEnvShape (f, s.curves[i]);
            }
            prevT = segT;
            prevL = s.levels[i];
        }
        return s.levels[last];
    }

    const EnvSnapshot* snap { nullptr };
    Stage  stage { Stage::Idle };
    double sr { 44100.0 };
    double timeScale { 1.0 };
    double timePos { 0.0 };
    double releaseClock { 0.0 };
    float  value { 0.0f };
    float  releaseStartValue { 0.0f };
};

class FiSynthVoice : public juce::SynthesiserVoice
{
public:
    // Cele modulacji obwiedni (źródło = obwiednia). Kolejność musi zgadzać się
    // z listą Choice "envNDest" w PluginProcessor (nowe cele TYLKO na końcu).
    enum ModDest { ModNone = 0, ModCutoff, ModPitch, ModOscMix, ModResonance,
                   ModStretch1, ModStretch2, ModStretch3,
                   ModFM, ModTilt1, ModTilt2, ModTilt3 };
    static constexpr int numModSlots = 3;

    FiSynthVoice() = default;

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<FiSynthSound*> (sound) != nullptr;
    }

    // stretchMode jest CIĄGŁY (0..8): część ułamkowa crossfade'uje między
    // sąsiednimi celami stretcha (morph z XY pada); wartości całkowite brzmią
    // jak dawne tryby dyskretne.
    void setOscillatorParams (int oscIndex, int waveform, float detuneAmount, float mix,
                              float stretch, float stretchMode = 0.0f, float coarseCents = 0.0f,
                              float tilt = 1.0f)
    {
        if (oscIndex >= 0 && oscIndex < 3)
        {
            // Cache amplitud (waveform+tilt) odświeża się leniwie w renderze —
            // tam, gdzie znany jest też wkład modulacji tiltu (ModTilt).
            oscs[oscIndex].waveform = juce::jlimit (0, 12, waveform);
            oscs[oscIndex].detune = detuneAmount;
            oscs[oscIndex].coarseCents = coarseCents;
            oscs[oscIndex].mix = juce::jlimit (0.0f, 1.0f, mix);
            oscs[oscIndex].stretch = juce::jlimit (0.0f, 1.0f, stretch);
            oscs[oscIndex].stretchMode = juce::jlimit (0.0f, (float) (fiNumStretchModes - 1),
                                                       stretchMode);
            oscs[oscIndex].tilt = fiMod::clampTilt (tilt);
        }
    }

    // === Parametry "złotego" silnika (wspólne dla głosu) ===
    // fm     — indeks Golden FM: sinus o częstotliwości f·φ moduluje przyrosty
    //          faz WSZYSTKICH partiali (proporcjonalna dewiacja = stały indeks).
    // ring   — dry/wet ring-modu osc1×osc2 (sidebandy sum/różnic wszystkich par
    //          partiali; przy goldint=φ maksymalnie gęste i nieokresowe).
    // sub    — poziom sub-partiala na f/φ (złota "sub-oktawa", −833.09¢).
    // uni    — Golden Unison: dolne partialki dostają bliźniaka rozstrojonego
    //          o frac(n·φ) (low-discrepancy detune, chór bez okresowego phasingu).
    // quant  — kwantyzacja modulacji Pitch do krotności złotego interwału 833.09¢.
    void setGoldenParams (float fm, float ring, float sub, float uni, bool quant) noexcept
    {
        fmAmount   = juce::jlimit (0.0f, 1.0f, fm);
        ringMix    = juce::jlimit (0.0f, 1.0f, ring);
        subLevel   = juce::jlimit (0.0f, 1.0f, sub);
        unisonAmt  = juce::jlimit (0.0f, 1.0f, uni);
        pitchQuant = quant;

        // Mnożniki rozstrojenia bliźniaków: frac((n+1)/φ) zmapowane na [-1,1],
        // maks. ±0.6% (~±10¢). Stałe w bloku — liczone tu, nie w gorącej pętli.
        for (int n = 0; n < kUniPartials; ++n)
        {
            const double g    = (n + 1) / fiPhi;
            const double frac = g - std::floor (g);
            uniFactor[n] = 1.0 + (2.0 * frac - 1.0) * 0.006 * (double) unisonAmt;
        }
    }

    // Szerokość stereo phyllotaxis: partial n panoramowany do frac(n/φ)·spread.
    void setStereoSpread (float s) noexcept { stereoSpread = juce::jlimit (0.0f, 1.0f, s); }

    // idx 0 = obwiednia amplitudy (VCA), idx 1..3 = obwiednie modulacyjne.
    void setEnvelope (int idx, const EnvSnapshot* snapshot)
    {
        if (idx >= 0 && idx < kNumEnvelopes)
            env[idx].setSnapshot (snapshot);
    }

    // Skala czasu obwiedni (sync do tempa) — wspólna dla wszystkich obwiedni.
    void setEnvTimeScale (double s)
    {
        for (auto& e : env)
            e.setTimeScale (s);
    }

    void setEnvMod (int slot, int dest, float amount)
    {
        if (slot >= 0 && slot < numModSlots)
        {
            modSlots[slot].dest   = juce::jlimit (0, (int) ModTilt3, dest);
            modSlots[slot].amount = juce::jlimit (-1.0f, 1.0f, amount);
        }
    }

    void setFilterParams (float cutoff, float resonance, int type)
    {
        filterCutoff = fiMod::clampCutoff (cutoff);
        filterResonance = fiMod::clampResonance (resonance);
        filterType = juce::jlimit (0, 2, type);
    }

    // drift — Golden Drift: miks z drugim LFO o rate·φ. Suma dwóch przebiegów
    // w niewymiernym stosunku częstotliwości nigdy się nie powtarza
    // (quasi-periodyczny ruch jak drift analogu).
    void setLFOParams (float rate, float depth, int shape, float drift = 0.0f)
    {
        lfoRate = juce::jlimit (0.01f, 40.0f, rate);
        lfoDepth = juce::jlimit (0.0f, 1.0f, depth);
        lfoShape = juce::jlimit (0, 3, shape);
        lfoDrift = juce::jlimit (0.0f, 1.0f, drift);

        // Rate może zmieniać się na żywo (sync do tempa, zmiana BPM), więc
        // angleDelta przeliczamy tutaj — nie tylko w startNote.
        if (getSampleRate() > 0.0)
            lfoAngleDelta = (lfoRate / getSampleRate()) * juce::MathConstants<double>::twoPi;
    }

    // --- Playhead: odczytywane przez procesor (wątek audio) ---
    // Głos żyje dopóki gra obwiednia amplitudy (env[0]).
    bool  isEnvActive() const noexcept { return env[0].isActive(); }
    float getEnvTime (int idx) const noexcept
    {
        return (idx >= 0 && idx < kNumEnvelopes) ? env[idx].absTime() : -1.0f;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override
    {
        level = velocity * 0.15f;
        pitchWheelMoved (currentPitchWheelPosition);
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

        for (int o = 0; o < 3; ++o)
        {
            // Całkowite przestrojenie w centach = gruby złoty interwał (k·833.09¢)
            // + fine detune (±48¢). baseFreq = f·2^(centy/1200).
            const float totalCents = oscs[o].coarseCents + oscs[o].detune;
            oscs[o].baseFreq = frequency * std::pow (2.0, (double) totalCents / 1200.0);

            for (int n = 0; n < numPartials; ++n)
            {
                // Fazy startowe rozłożone kątem złotym (frac(n/φ)·2π) zamiast 0.
                // Wszystkie partialki zsynchronizowane w fazie 0 dają wysoki pik
                // chwilowy na ataku ("klik"); rozkład złoty maksymalnie równo
                // dekoreluje partialki -> niższy crest factor, gładszy atak.
                // Deterministyczne: ta sama nuta = ta sama faza startowa.
                const double goldenStep = n / fiPhi;
                const double frac = goldenStep - std::floor (goldenStep);     // część ułamkowa
                oscs[o].currentPhase[n] = frac * juce::MathConstants<double>::twoPi;
            }

            // Częstotliwości partialek to baseFreq·ratio ze wspólnej, policzonej
            // raz tablicy (ratioRow) — zostaje samo przeliczenie delt pod stretch.
            updateStretchDeltas (o, oscs[o].stretch);

            // Bliźniaki unisono startują z innym rozkładem faz niż partialki
            // główne (przesunięcie o φ w indeksie), żeby para nie ruszała
            // idealnie zsynchronizowana.
            for (int n = 0; n < kUniPartials; ++n)
            {
                const double g = (n + fiPhi) / fiPhi;
                oscs[o].uniPhase[n] = (g - std::floor (g)) * juce::MathConstants<double>::twoPi;
            }
        }

        const double k = juce::MathConstants<double>::twoPi / getSampleRate();

        // Golden FM: modulator to jeden sinus na głos o częstotliwości f·φ.
        fmPhase = 0.0;
        fmDelta = frequency * fiPhi * k;

        // Golden Sub: sub-partial na f/φ (podąża za detune/goldint oscylatora 1).
        subPhase = 0.0;
        subDelta = (oscs[0].baseFreq / fiPhi) * k;

        lfoPhase = 0.0;
        lfoPhase2 = 0.0;
        lfoStep = 0;
        lfoStep2 = 0;
        lfoAngleDelta = (lfoRate / getSampleRate()) * juce::MathConstants<double>::twoPi;

        // Świeży stan filtra — przy wysokim rezonansie "ring" poprzedniej nuty
        // wjeżdżał w atak nowej.
        filter.reset();

        for (auto& e : env)
            e.noteOn();
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            for (auto& e : env)
                e.noteOff();
        }
        else
        {
            clearCurrentNote();
            for (auto& e : env)
                e.reset();
        }
    }

    // Kółko: 0..16383, środek 8192 → ±bendRangeSemis. Wywoływane z wątku audio
    // (Synthesiser tnie blok na granicach eventów MIDI), więc zwykły float.
    void pitchWheelMoved (int newValue) override
    {
        bendSemis = (float) (newValue - 8192) / 8192.0f * fiMod::bendRangeSemis;
    }
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (! env[0].isActive())
            return;

        bool hasFmMod = false;
        for (int m = 0; m < numModSlots; ++m)
            if (modSlots[m].dest == ModFM)
                hasFmMod = true;

        // Stretch przeliczamy w tempie kontrolnym (co 32 próbki) — nie potrzebuje
        // rozdzielczości audio, a to ~32x tańsze niż przeliczanie co próbkę.
        // Typ filtra jest stały w obrębie bloku — ustaw raz, nie co próbkę.
        switch (filterType)
        {
            case 0: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
            case 1: filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
            case 2: filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
        }

        // Współczynniki filtra (drogie tan() w setCutoffFrequency) oraz stretch
        // przeliczamy w tempie kontrolnym — nie potrzebują rozdzielczości audio.
        constexpr int controlInterval = 32;

        // Wskaźniki kanałów raz na blok — addSample robi bounds-check per próbkę.
        auto* const* outChans = outputBuffer.getArrayOfWritePointers();
        const int    numCh    = outputBuffer.getNumChannels();

        // Pan phyllotaxis per partial: fundament w centrum, wyżej frac(n/φ)
        // zmapowane na [-1,1] — równomierne pole stereo bez okresowych zbitek.
        // Wzmocnienia constant-power przeliczane TYLKO przy zmianie spread
        // (32×sqrt nie ma czego robić co blok).
        if (! juce::exactlyEqual (stereoSpread, cachedSpread))
        {
            cachedSpread = stereoSpread;
            for (int n = 0; n < numPartials; ++n)
            {
                const float p = fiGoldenPan (n) * stereoSpread;
                gainL[n] = std::sqrt (1.0f - p);
                gainR[n] = std::sqrt (1.0f + p);
            }
        }

        // Ścieżka mono (spread≈0, domyślne): jedna akumulacja, wspólny sygnał
        // dla L/R — połowa MAC-ów w gorącej pętli partiali. Filtr liczy oba
        // kanały zawsze, żeby stan R był grzany (bez kliku przy włączaniu spread).
        const bool stereo = stereoSpread > 1.0e-4f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // env[0] napędza głośność (VCA); env[1..3] to źródła modulacji.
            const float ampVal = env[0].nextSample();

            float modVal[numModSlots];
            for (int m = 0; m < numModSlots; ++m)
                modVal[m] = env[m + 1].nextSample();

            // Mody wpływające na oscylatory liczymy co próbkę (czułe na schodkowanie).
            float pitchSemis = 0.0f;   // przesunięcie wysokości
            float mixMul     = 1.0f;   // skala miksu oscylatorów
            float fmAdd      = 0.0f;   // dodatek do indeksu Golden FM
            float stretchAdd[3] = { 0.0f, 0.0f, 0.0f };   // dodatek do stretcha, osobno per osc

            for (int m = 0; m < numModSlots; ++m)
            {
                const float a = modVal[m] * modSlots[m].amount;
                switch (modSlots[m].dest)
                {
                    case ModPitch:     pitchSemis += a * fiMod::pitchRangeSemis; break;
                    case ModOscMix:    mixMul     *= fiMod::gainFactor (a);      break;
                    case ModFM:        fmAdd      += a;                          break;
                    case ModStretch1:  stretchAdd[0] += a;                       break;
                    case ModStretch2:  stretchAdd[1] += a;                       break;
                    case ModStretch3:  stretchAdd[2] += a;                       break;
                    default: break;  // cutoff/rezonans/tilt — w tempie kontrolnym poniżej
                }
            }

            // Kwantyzacja pitch-modu do siatki złotego interwału: obwiednia
            // przemiatająca Pitch gra wtedy schodki k·833.09¢ (spójne z goldint).
            if (pitchQuant && std::abs (pitchSemis) > 1.0e-6f)
            {
                constexpr float goldSemis = fiGoldenIntervalCents / 100.0f;
                pitchSemis = std::round (pitchSemis / goldSemis) * goldSemis;
            }

            // Pitch bend z kontrolera PO kwantyzacji — kółko ma być płynne,
            // siatka złotych interwałów dotyczy tylko modulacji z obwiedni.
            pitchSemis += bendSemis;

            // Tempo kontrolne: stretch, tilt + współczynniki filtra (mod cutoff/rezonans + LFO).
            if ((sample % controlInterval) == 0)
            {
                // Delty przeliczamy przy KAŻDEJ zmianie efektywnego stretcha lub
                // trybu — także z gałki/spirali w trakcie trzymanej nuty, nie
                // tylko z modulacji. Dwa porównania per osc = ścieżka bezczynna
                // dalej prawie darmowa, a spirala przestaje pokazywać widmo,
                // którego nuta nie gra.
                for (int o = 0; o < 3; ++o)
                {
                    const float effStretch = fiMod::stretchTarget (oscs[o].stretch, stretchAdd[o]);
                    if (std::abs (effStretch - oscs[o].appliedStretch) > 1.0e-4f
                        || std::abs (oscs[o].stretchMode - oscs[o].appliedMode) > 1.0e-4f)
                        updateStretchDeltas (o, effStretch);
                }

                float cutoffMul = 1.0f;   // mnożnik (oktawy)
                float resMul    = 1.0f;   // skala rezonansu
                float tiltAdd[3] = { 0.0f, 0.0f, 0.0f };
                for (int m = 0; m < numModSlots; ++m)
                {
                    const float a = modVal[m] * modSlots[m].amount;
                    if      (modSlots[m].dest == ModCutoff)    cutoffMul *= fiMod::cutoffFactor (a);
                    else if (modSlots[m].dest == ModResonance) resMul    *= fiMod::gainFactor (a);
                    else if (modSlots[m].dest >= ModTilt1 && modSlots[m].dest <= ModTilt3)
                        tiltAdd[modSlots[m].dest - ModTilt1] += a;
                }

                // Golden Tilt: cache amplitud odświeżany tylko gdy waveform lub
                // efektywny tilt (parametr + mod) faktycznie się zmienił.
                for (int o = 0; o < 3; ++o)
                    refreshHarmonicAmps (o, fiMod::tiltTarget (oscs[o].tilt, tiltAdd[o]));

                const float lfoValue = getLFOValue();
                const float cutoffModulated = fiMod::clampCutoff (
                    filterCutoff * (1.0f + lfoValue * lfoDepth * 0.5f) * cutoffMul);

                filter.setCutoffFrequency (cutoffModulated);
                filter.setResonance (fiMod::clampResonance (filterResonance * resMul));
            }

            const float pitchFactor = (std::abs (pitchSemis) > 1.0e-6f)
                                     ? std::exp2 (pitchSemis / 12.0f) : 1.0f;

            // Golden FM: wspólny czynnik częstotliwości dla wszystkich partiali
            // (dewiacja proporcjonalna do f_n = stały indeks modulacji). Wchodzi
            // w freqFactor razem z pitch-modem — zero dodatkowego kosztu w pętli.
            const float fmEff = fiMod::fmTarget (fmAmount, hasFmMod ? fmAdd : 0.0f);
            float fmFactor = 1.0f;
            if (fmEff > 1.0e-4f)
                fmFactor = 1.0f + fmEff * 0.5f * std::sin ((float) fmPhase);

            // Przy mocnym pitch-modzie przyrost potrafi przekroczyć 2π —
            // pojedyncze odejmowanie nie wystarcza, faza rosłaby bez końca
            // i cast do float w sin() traciłby precyzję (szum zamiast tonu).
            fmPhase += fmDelta * pitchFactor;
            if (fmPhase >= juce::MathConstants<double>::twoPi)
                fmPhase = std::fmod (fmPhase, juce::MathConstants<double>::twoPi);

            const float freqFactor = pitchFactor * fmFactor;

            const bool wantRing = ringMix > 1.0e-4f;
            const bool wantUni  = unisonAmt > 1.0e-4f;
            float ringSrcL[2] = { 0.0f, 0.0f };
            float ringSrcR[2] = { 0.0f, 0.0f };

            float mixL = 0.0f, mixR = 0.0f;
            for (int o = 0; o < 3; ++o)
            {
                // Ring-mod potrzebuje osc1/osc2 nawet przy mix=0 (klasyka:
                // modulator słyszalny tylko przez produkt).
                const bool ringSrc = wantRing && o < 2;
                if (oscs[o].mix <= 0.0f && ! ringSrc) continue;

                float outL = 0.0f, outR = 0.0f;
                if (oscs[o].waveform == 5)
                {
                    const float s = noise.nextFloat() * 2.0f - 1.0f;   // szum w centrum
                    outL = s;
                    outR = s;
                }
                else
                {
                    // Granica aliasingu: partial ponad Nyquistem (delta >= π) jest
                    // pomijany — grałby wyłącznie jako alias. Quadratic gra cos(2θ),
                    // czyli oktawę wyżej, więc jego granica to π/2.
                    const double maxDelta = (oscs[o].waveform == 4)
                        ? juce::MathConstants<double>::halfPi
                        : juce::MathConstants<double>::pi;

                    // Górne 8% pasma wygaszane liniowo — przy audio-rate zmianach
                    // częstotliwości (Golden FM, pitch-mod) partial nie wpada
                    // i nie wypada zerojedynkowo na granicy (kliki), tylko cichnie.
                    const double fadeStart = maxDelta * 0.92;
                    const double invFade   = 1.0 / (maxDelta * 0.08);

                    for (int n = 0; n < numPartials; ++n)
                    {
                        const double delta = oscs[o].angleDelta[n] * freqFactor;
                        if (delta >= maxDelta)
                            continue;

                        // Partialki z zerową amplitudą (np. parzyste w square) nie
                        // potrzebują sin() — tylko faza musi lecieć dalej.
                        const float amp = oscs[o].harmonicAmp[n];
                        if (amp > 0.0f)
                        {
                            float s = (oscs[o].waveform == 4)
                                ? amp * std::cos (2.0f * (float) oscs[o].currentPhase[n])
                                : amp * std::sin ((float) oscs[o].currentPhase[n]);
                            if (delta > fadeStart)
                                s *= (float) ((maxDelta - delta) * invFade);
                            if (stereo)
                            {
                                outL += s * gainL[n];
                                outR += s * gainR[n];
                            }
                            else
                                outL += s;
                        }

                        // Faza w [0, 2π): bez zawijania rośnie bez ograniczeń i cast
                        // do float traci precyzję (po ~1 min słychać szum/detune).
                        double& ph = oscs[o].currentPhase[n];
                        ph += delta;
                        if (ph >= juce::MathConstants<double>::twoPi)
                            ph -= juce::MathConstants<double>::twoPi;
                    }

                    // Golden Unison: dolne partialki dostają bliźniaka rozstrojonego
                    // kątem złotym (uniFactor), z zamienionymi wzmocnieniami pan
                    // (L↔R) — dudnienie w niewymiernych stosunkach + szerokość.
                    if (wantUni)
                    {
                        const float twinGain = 0.7f * unisonAmt;
                        for (int n = 0; n < kUniPartials; ++n)
                        {
                            const double delta = oscs[o].angleDelta[n] * uniFactor[n] * freqFactor;
                            if (delta >= maxDelta)
                                continue;

                            const float amp = oscs[o].harmonicAmp[n];
                            if (amp > 0.0f)
                            {
                                float s = twinGain * ((oscs[o].waveform == 4)
                                    ? amp * std::cos (2.0f * (float) oscs[o].uniPhase[n])
                                    : amp * std::sin ((float) oscs[o].uniPhase[n]));
                                if (delta > fadeStart)
                                    s *= (float) ((maxDelta - delta) * invFade);
                                if (stereo)
                                {
                                    outL += s * gainR[n];   // pan odbity względem głównego
                                    outR += s * gainL[n];
                                }
                                else
                                    outL += s;
                            }

                            double& ph = oscs[o].uniPhase[n];
                            ph += delta;
                            if (ph >= juce::MathConstants<double>::twoPi)
                                ph -= juce::MathConstants<double>::twoPi;
                        }
                    }
                }

                if (ringSrc)
                {
                    ringSrcL[o] = outL;
                    ringSrcR[o] = stereo ? outR : outL;
                }
                mixL += outL * oscs[o].mix * mixMul;
                if (stereo)
                    mixR += outR * oscs[o].mix * mixMul;
            }

            // Golden Ring: produkt osc1·osc2 dosypywany do miksu. Sidebandy
            // f1±f2 wszystkich par partiali — przy goldint=φ nieharmoniczne
            // widmo o maksymalnej gęstości (dzwony/metal) za jedno mnożenie.
            if (wantRing)
            {
                mixL += ringMix * ringSrcL[0] * ringSrcL[1];
                if (stereo)
                    mixR += ringMix * ringSrcR[0] * ringSrcR[1];
            }

            // Golden Sub: sinus na f/φ w centrum (faza leci zawsze — podkręcenie
            // gałki w trakcie nuty nie strzela nieciągłością).
            if (subLevel > 1.0e-4f)
            {
                const float s = subLevel * std::sin ((float) subPhase);
                mixL += s;
                if (stereo)
                    mixR += s;
            }
            subPhase += subDelta * freqFactor;
            if (subPhase >= juce::MathConstants<double>::twoPi)
                subPhase = std::fmod (subPhase, juce::MathConstants<double>::twoPi);

            const float inR = stereo ? mixR : mixL;
            const float fL = filter.processSample (0, mixL);
            const float fR = filter.processSample (1, inR);

            const float vca  = level * ampVal * 0.3f;
            const float resL = fL * vca;
            const float resR = fR * vca;

            if (numCh >= 2)
            {
                outChans[0][startSample + sample] += resL;
                outChans[1][startSample + sample] += resR;
                for (int ch = 2; ch < numCh; ++ch)
                    outChans[ch][startSample + sample] += (resL + resR) * 0.5f;
            }
            else if (numCh == 1)
                outChans[0][startSample + sample] += (resL + resR) * 0.5f;

            lfoPhase += lfoAngleDelta;
            if (lfoPhase >= juce::MathConstants<double>::twoPi)
            {
                lfoPhase -= juce::MathConstants<double>::twoPi;
                ++lfoStep;   // licznik cykli — napędza kształt Golden S&H
            }

            // Drugi LFO (Golden Drift) biegnie z rate·φ — miks w getLFOValue.
            lfoPhase2 += lfoAngleDelta * fiPhi;
            if (lfoPhase2 >= juce::MathConstants<double>::twoPi)
            {
                lfoPhase2 -= juce::MathConstants<double>::twoPi;
                ++lfoStep2;
            }
        }

        if (! env[0].isActive())
            clearCurrentNote();
    }

    void setCurrentPlaybackSampleRate (double sampleRate) override
    {
        if (sampleRate > 0.0)
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sampleRate);
            for (auto& e : env)
                e.setSampleRate (sampleRate);

            // Filtr musi znać realny sample rate (g = tan(π·fc/sr)); bez tego
            // trzymał domyślne 44100 i rozstrajał cutoff. 2 kanały: tor jest
            // stereo od pan-u phyllotaxis (L/R filtrowane tymi samymi współcz.).
            juce::dsp::ProcessSpec spec { sampleRate, 512u, 2u };
            filter.prepare (spec);
        }
    }

private:
    // Tablice ratio/amplitud partiali są wspólne z GUI — patrz PartialTables.h.
    static constexpr int numPartials = fiNumPartials;

    // Liczba dolnych partiali z bliźniakiem unisono (wyżej dudnienie i tak
    // maskuje gęstość widma, a koszt rośnie liniowo).
    static constexpr int kUniPartials = 8;

    struct Oscillator
    {
        double currentPhase[numPartials] { 0.0 };
        double angleDelta[numPartials]   { 0.0 };
        double uniPhase[kUniPartials]    { 0.0 };  // fazy bliźniaków Golden Unison
        double baseFreq { 0.0 };                   // fundament po detune + goldint (Hz)
        float  harmonicAmp[numPartials]  { 0.0f }; // amplitudy harmonicznych (cache: waveform+tilt)
        int waveform { 0 };
        int   appliedWave { -1 };                  // -1 wymusza wypełnienie cache przy 1. renderze
        float appliedTilt { -1.0f };
        float detune { 0.0f };
        float coarseCents { 0.0f };                // gruby złoty interwał (k·833.09¢)
        float mix { 0.333f };
        float stretch { 0.0f };
        float stretchMode { 0.0f };                // ciągłe 0..8: 0=Golden 1=Fib 2=GoldOct 3=GoldDet 4=Silver 5=Bronze 6=GoldStiff 7=Lucas 8=GoldShift; ułamek = morph
        float tilt { 1.0f };                       // Golden Tilt: wykładnik opadania amplitud
        float appliedStretch { -1.0f };            // wartości, dla których policzono angleDelta
        float appliedMode    { -1.0f };            //   (-1 wymusza przeliczenie w 1. bloku)
    } oscs[3];

    // Golden Tilt: amplitudy = baza waveformu · (n+1)^(1−tilt). tilt=1 to czysty
    // waveform, tilt=φ przyciemnia po złotym wykładniku, <1 rozjaśnia. Cache
    // odświeżany tylko przy realnej zmianie (waveform lub |Δtilt| > 1e-3).
    void refreshHarmonicAmps (int o, float effTilt) noexcept
    {
        auto& osc = oscs[o];
        if (osc.waveform == osc.appliedWave
            && std::abs (effTilt - osc.appliedTilt) < 1.0e-3f)
            return;

        for (int n = 0; n < numPartials; ++n)
        {
            const float base = fiHarmonicAmplitude (osc.waveform, n);
            osc.harmonicAmp[n] = (base > 0.0f)
                ? base * std::pow ((float) (n + 1), 1.0f - effTilt)
                : 0.0f;
        }
        osc.appliedWave = osc.waveform;
        osc.appliedTilt = effTilt;
    }

    // Przelicza angleDelta partialek oscylatora o pod zadany stretch — ratio
    // liczy wspólny fiStretchedRatios (jedyna definicja blendu morpha,
    // ta sama co na spirali).
    void updateStretchDeltas (int o, float stretch) noexcept
    {
        const double f0k = oscs[o].baseFreq
                         * juce::MathConstants<double>::twoPi / getSampleRate();

        double ratios[numPartials];
        fiStretchedRatios (oscs[o].stretchMode, stretch, ratios);
        for (int n = 0; n < numPartials; ++n)
            oscs[o].angleDelta[n] = f0k * ratios[n];

        oscs[o].appliedStretch = stretch;
        oscs[o].appliedMode    = juce::jlimit (0.0f, (float) (fiNumStretchModes - 1),
                                               oscs[o].stretchMode);
    }

    double frequency { 0.0 };
    float level { 0.0f };
    float stereoSpread { 0.0f };
    float bendSemis { 0.0f };   // pitch bend z kółka, w półtonach (±bendRangeSemis)

    // === Złoty silnik (per głos) ===
    float fmAmount  { 0.0f };   // indeks Golden FM (modulator f·φ)
    float ringMix   { 0.0f };   // dry/wet ring-modu osc1×osc2
    float subLevel  { 0.0f };   // poziom sub-partiala f/φ
    float unisonAmt { 0.0f };   // Golden Unison (bliźniaki dolnych partiali)
    bool  pitchQuant { false }; // kwantyzacja pitch-modu do 833.09¢
    double fmPhase { 0.0 }, fmDelta { 0.0 };
    double subPhase { 0.0 }, subDelta { 0.0 };
    double uniFactor[kUniPartials] { 1.0 };  // mnożniki rozstrojenia bliźniaków

    // Cache wzmocnień pan (constant-power z fiGoldenPan) — przeliczany
    // w renderNextBlock tylko gdy spread się zmienił.
    float cachedSpread { -1.0f };
    float gainL[numPartials] {};
    float gainR[numPartials] {};

    // Filter
    juce::dsp::StateVariableTPTFilter<float> filter;
    float filterCutoff { 5000.0f };
    float filterResonance { 1.0f };
    int filterType { 0 };

    // LFO (drugi bieg fazowy z rate·φ dla Golden Drift; liczniki cykli dla S&H)
    double lfoPhase { 0.0 }, lfoPhase2 { 0.0 };
    double lfoAngleDelta { 0.0 };
    int lfoStep { 0 }, lfoStep2 { 0 };
    float lfoRate { 1.0f };
    float lfoDepth { 0.0f };
    float lfoDrift { 0.0f };
    int lfoShape { 0 };

    // Szum per-głos — getSystemRandom() jest współdzielony między wątkami
    // (audio vs message), więc nie wolno go używać w renderze.
    juce::Random noise;

    // Obwiednie: env[0] = amplituda (VCA), env[1..3] = modulatory.
    EnvPlayer env[kNumEnvelopes];

    struct ModSlot { int dest { ModNone }; float amount { 0.0f }; };
    ModSlot modSlots[numModSlots];

    static float lfoShapeValue (int shape, double phase, int step) noexcept
    {
        const float t = (float) (phase / juce::MathConstants<double>::twoPi);
        switch (shape)
        {
            case 0:  // Sine
                return std::sin ((float) phase);
            case 1:  // Square
                return t < 0.5f ? 1.0f : -1.0f;
            case 2:  // Triangle
                return t < 0.5f ? -1.0f + 4.0f * t : 3.0f - 4.0f * t;
            case 3:  // Golden S&H — schodki ciągu Weyla frac(k/φ): "random",
            {        // który nigdy się nie powtarza i równomiernie pokrywa zakres.
                const double g = step / fiPhi;
                return (float) (2.0 * (g - std::floor (g)) - 1.0);
            }
            default:
                return 0.0f;
        }
    }

    float getLFOValue() const noexcept
    {
        const float v1 = lfoShapeValue (lfoShape, lfoPhase, lfoStep);
        if (lfoDrift <= 1.0e-4f)
            return v1;

        // Golden Drift: miks z drugim LFO o rate·φ — stosunek niewymierny,
        // więc suma jest quasi-periodyczna (nigdy nie wraca do tej samej fazy).
        const float v2 = lfoShapeValue (lfoShape, lfoPhase2, lfoStep2);
        return v1 + 0.5f * lfoDrift * (v2 - v1);
    }
};
