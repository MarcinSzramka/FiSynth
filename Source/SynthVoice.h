#pragma once

#include <JuceHeader.h>
#include "SynthSound.h"
#include "EnvelopeModel.h"

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
                value = evalRelease (s, (float) releaseClock);
                releaseClock += dt;
                const float relLen = s.times[s.numPoints - 1] - s.times[s.sustainIndex];
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
            default:                return -1.0f;   // Idle
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
    // Cele modulacji obwiedni (źródło = obwiednia).
    enum ModDest { ModNone = 0, ModCutoff, ModPitch, ModOscMix, ModResonance,
                   ModStretch1, ModStretch2, ModStretch3 };
    static constexpr int numModSlots = 3;

    FiSynthVoice() = default;

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<FiSynthSound*> (sound) != nullptr;
    }

    void setOscillatorParams (int oscIndex, int waveform, float detuneAmount, float mix, float stretch)
    {
        if (oscIndex >= 0 && oscIndex < 3)
        {
            const int wf = juce::jlimit (0, 5, waveform);

            // Amplitudy harmonicznych zależą tylko od waveformu — przelicz cache
            // gdy się zmienił (raz na blok, nie co próbkę w renderze).
            if (wf != oscs[oscIndex].waveform)
                for (int n = 0; n < numPartials; ++n)
                    oscs[oscIndex].harmonicAmp[n] = getHarmonicAmplitude (wf, n);

            oscs[oscIndex].waveform = wf;
            oscs[oscIndex].detune = detuneAmount;
            oscs[oscIndex].mix = juce::jlimit (0.0f, 1.0f, mix);
            oscs[oscIndex].stretch = juce::jlimit (0.0f, 1.0f, stretch);
        }
    }

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
            modSlots[slot].dest   = juce::jlimit (0, (int) ModStretch3, dest);
            modSlots[slot].amount = juce::jlimit (-1.0f, 1.0f, amount);
        }
    }

    void setFilterParams (float cutoff, float resonance, int type)
    {
        filterCutoff = juce::jlimit (20.0f, 20000.0f, cutoff);
        filterResonance = juce::jlimit (0.1f, 10.0f, resonance);
        filterType = juce::jlimit (0, 2, type);
    }

    void setLFOParams (float rate, float depth, int shape)
    {
        lfoRate = juce::jlimit (0.01f, 40.0f, rate);
        lfoDepth = juce::jlimit (0.0f, 1.0f, depth);
        lfoShape = juce::jlimit (0, 2, shape);

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
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        level = velocity * 0.15f;
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

        constexpr float phi = 1.618034f;

        for (int o = 0; o < 3; ++o)
        {
            float detuneHz = frequency * (std::pow (2.0f, oscs[o].detune / 1200.0f) - 1.0f);
            float oscFreq = frequency + detuneHz;

            for (int n = 0; n < numPartials; ++n)
            {
                oscs[o].currentPhase[n] = 0.0;

                float freqHarmonic = oscFreq * (n + 1.0f);
                float freqPhi = oscFreq * std::pow (phi, (float)n);

                // Zapamiętujemy obie skrajne częstotliwości partialki — to pozwala
                // tanio przeliczać angleDelta przy modulacji stretcha (lerp między nimi).
                oscs[o].freqHarmonic[n] = freqHarmonic;
                oscs[o].freqPhi[n]      = freqPhi;

                float freqStretched = freqHarmonic + (freqPhi - freqHarmonic) * oscs[o].stretch;

                oscs[o].angleDelta[n] = (freqStretched / getSampleRate()) * juce::MathConstants<double>::twoPi;
            }
        }

        lfoPhase = 0.0;
        lfoAngleDelta = (lfoRate / getSampleRate()) * juce::MathConstants<double>::twoPi;

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

    void pitchWheelMoved (int) override     {}
    void controllerMoved (int, int) override {}

    float getHarmonicAmplitude (int waveform, int n)
    {
        float baseAmp = 1.0f / std::sqrt (1.0f + n);

        switch (waveform)
        {
            case 0:  // Sine
                return (n == 0) ? 1.0f : 0.0f;

            case 1:  // Square
                if (n % 2 == 0) return 0.0f;
                return baseAmp / (n + 1.0f);

            case 2:  // Triangle
                if (n % 2 == 0) return 0.0f;
                return baseAmp / ((n + 1.0f) * (n + 1.0f));

            case 3:  // Sawtooth
                return baseAmp / (n + 1.0f);

            case 4:  // Quadratic
                if (n % 2 == 1) return 0.0f;
                return baseAmp / (n + 1.0f);

            case 5:  // Noise
                return baseAmp * 0.1f;

            default:
                return baseAmp;
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (! env[0].isActive())
            return;

        // Czy którykolwiek slot moduluje stretch? Jeśli nie — nie ruszamy
        // angleDelta w ogóle (ścieżka zerokosztowa, jak dotychczas).
        bool hasStretchMod = false;
        for (int m = 0; m < numModSlots; ++m)
            if (modSlots[m].dest >= ModStretch1 && modSlots[m].dest <= ModStretch3)
                hasStretchMod = true;

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
            float stretchAdd[3] = { 0.0f, 0.0f, 0.0f };   // dodatek do stretcha, osobno per osc

            for (int m = 0; m < numModSlots; ++m)
            {
                const float a = modVal[m] * modSlots[m].amount;
                switch (modSlots[m].dest)
                {
                    case ModPitch:     pitchSemis += a * 24.0f;                 break;  // ±24 półtony
                    case ModOscMix:    mixMul     *= juce::jmax (0.0f, 1.0f + a); break;
                    case ModStretch1:  stretchAdd[0] += a;                       break;
                    case ModStretch2:  stretchAdd[1] += a;                       break;
                    case ModStretch3:  stretchAdd[2] += a;                       break;
                    default: break;  // cutoff/rezonans — w tempie kontrolnym poniżej
                }
            }

            // Tempo kontrolne: stretch + współczynniki filtra (mod cutoff/rezonans + LFO).
            if ((sample % controlInterval) == 0)
            {
                if (hasStretchMod)
                    for (int o = 0; o < 3; ++o)
                        updateStretchDeltas (o, juce::jlimit (0.0f, 1.0f, oscs[o].stretch + stretchAdd[o]));

                float cutoffMul = 1.0f;   // mnożnik (oktawy)
                float resMul    = 1.0f;   // skala rezonansu
                for (int m = 0; m < numModSlots; ++m)
                {
                    const float a = modVal[m] * modSlots[m].amount;
                    if      (modSlots[m].dest == ModCutoff)    cutoffMul *= std::pow (2.0f, a * 4.0f);
                    else if (modSlots[m].dest == ModResonance) resMul    *= juce::jmax (0.0f, 1.0f + a);
                }

                const float lfoValue = getLFOValue();
                float cutoffModulated = filterCutoff * (1.0f + lfoValue * lfoDepth * 0.5f) * cutoffMul;
                cutoffModulated = juce::jlimit (20.0f, 20000.0f, cutoffModulated);

                filter.setCutoffFrequency (cutoffModulated);
                filter.setResonance (juce::jlimit (0.1f, 10.0f, filterResonance * resMul));
            }

            const float pitchFactor = (pitchSemis != 0.0f)
                                     ? std::pow (2.0f, pitchSemis / 12.0f) : 1.0f;

            float outMix = 0.0f;
            for (int o = 0; o < 3; ++o)
            {
                if (oscs[o].mix <= 0.0f) continue;

                float out = 0.0f;
                if (oscs[o].waveform == 5)
                    out = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                else
                {
                    for (int n = 0; n < numPartials; ++n)
                    {
                        const float amp = oscs[o].harmonicAmp[n];
                        if (oscs[o].waveform == 4)
                            out += amp * std::cos (2.0f * (float)oscs[o].currentPhase[n]);
                        else
                            out += amp * std::sin ((float)oscs[o].currentPhase[n]);
                        oscs[o].currentPhase[n] += oscs[o].angleDelta[n] * pitchFactor;
                    }
                }
                outMix += out * oscs[o].mix * mixMul;
            }

            float filtered = filter.processSample (0, outMix);

            const float result = filtered * level * ampVal * 0.3f;

            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample (ch, startSample + sample, result);

            lfoPhase += lfoAngleDelta;
            if (lfoPhase >= juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;
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
            // trzymał domyślne 44100 i rozstrajał cutoff na innych częstotliwościach.
            juce::dsp::ProcessSpec spec { sampleRate, 512u, 1u };
            filter.prepare (spec);
        }
    }

private:
    static constexpr int numPartials = 16;

    struct Oscillator
    {
        double currentPhase[numPartials] { 0.0 };
        double angleDelta[numPartials]   { 0.0 };
        double freqHarmonic[numPartials] { 0.0 };  // częstotliwość partialki przy stretch=0
        double freqPhi[numPartials]      { 0.0 };  // ...i przy stretch=1
        float  harmonicAmp[numPartials]  { 0.0f }; // amplitudy harmonicznych (cache per waveform)
        int waveform { -1 };                       // -1 wymusza wypełnienie cache przy 1. setOscillatorParams
        float detune { 0.0f };
        float mix { 0.333f };
        float stretch { 0.0f };
    } oscs[3];

    // Przelicza angleDelta partialek oscylatora o pod zadany stretch (lerp między
    // freqHarmonic a freqPhi). Tanie: bez pow/std::pow, tylko mnożenia.
    void updateStretchDeltas (int o, float stretch) noexcept
    {
        const double k = juce::MathConstants<double>::twoPi / getSampleRate();
        for (int n = 0; n < numPartials; ++n)
        {
            const double fs = oscs[o].freqHarmonic[n]
                            + (oscs[o].freqPhi[n] - oscs[o].freqHarmonic[n]) * (double) stretch;
            oscs[o].angleDelta[n] = fs * k;
        }
    }

    double frequency { 0.0 };
    float level { 0.0f };

    // Filter
    juce::dsp::StateVariableTPTFilter<float> filter;
    float filterCutoff { 5000.0f };
    float filterResonance { 1.0f };
    int filterType { 0 };

    // LFO
    double lfoPhase { 0.0 };
    double lfoAngleDelta { 0.0 };
    float lfoRate { 1.0f };
    float lfoDepth { 0.0f };
    int lfoShape { 0 };

    // Obwiednie: env[0] = amplituda (VCA), env[1..3] = modulatory.
    EnvPlayer env[kNumEnvelopes];

    struct ModSlot { int dest { ModNone }; float amount { 0.0f }; };
    ModSlot modSlots[numModSlots];

    float getLFOValue()
    {
        float t = static_cast<float> (lfoPhase / juce::MathConstants<double>::twoPi);
        switch (lfoShape)
        {
            case 0:  // Sine
                return std::sin (static_cast<float> (lfoPhase));
            case 1:  // Square
                return t < 0.5f ? 1.0f : -1.0f;
            case 2:  // Triangle
                return t < 0.5f ? -1.0f + 4.0f * t : 3.0f - 4.0f * t;
            default:
                return 0.0f;
        }
    }
};
