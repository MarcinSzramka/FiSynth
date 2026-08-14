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
    {
        auto* voice = new FiSynthVoice();
        fiVoices.push_back (voice);
        synth.addVoice (voice);
    }

    synth.addSound (new FiSynthSound());

    // Wymuś budowę tablicy słów Fibonacciego TERAZ (wątek komunikatów) —
    // leniwa inicjalizacja statyku alokuje i bierze lock, więc nie może odpalić
    // się pierwszy raz w processBlock (host bez edytora + automacja gateOn).
    (void) fibonacciWord (0);

    // Ten sam powód dla tablicy ratio partiali: w hostach headless (bez
    // edytora) pierwszy dotyk fiRatioRow to startNote na wątku audio.
    (void) fiRatioRow (0);

    // Cache surowych wskaźników parametrów — patrz komentarz przy ParamPtrs.
    auto rp = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    par.gain            = rp ("gain");
    par.tempoSync       = rp ("tempoSync");
    par.bpm             = rp ("bpm");
    par.envSync         = rp ("envSync");
    par.envSnap         = rp ("envSnap");
    par.envGrid         = rp ("envGrid");
    par.filterCutoff    = rp ("filterCutoff");
    par.filterResonance = rp ("filterResonance");
    par.filterType      = rp ("filterType");
    par.lfoRate         = rp ("lfoRate");
    par.lfoDepth        = rp ("lfoDepth");
    par.lfoShape        = rp ("lfoShape");
    par.lfoSync         = rp ("lfoSync");
    par.lfoRateDiv      = rp ("lfoRateDiv");

    for (int slot = 0; slot < 3; ++slot)
    {
        const juce::String prefix = "env" + juce::String (slot + 1);
        par.envDest[slot] = rp (prefix + "Dest");
        par.envAmt[slot]  = rp (prefix + "Amt");
    }

    par.gateOn    = rp ("gateOn");
    par.gateDepth = rp ("gateDepth");
    par.gateDiv   = rp ("gateDiv");
    par.gateGen   = rp ("gateGen");
    par.spread    = rp ("spread");

    par.ringMix    = rp ("ringMix");
    par.fmAmt      = rp ("fmAmt");
    par.subLevel   = rp ("subLevel");
    par.unison     = rp ("unison");
    par.lfoDrift   = rp ("lfoDrift");
    par.pitchQuant = rp ("pitchQuant");

    par.arpOn   = rp ("arpOn");
    par.arpDiv  = rp ("arpDiv");
    par.arpLen  = rp ("arpLen");
    par.arpMode = rp ("arpMode");
    par.arpWord = rp ("arpWord");
    par.arpVel  = rp ("arpVel");

    par.dlyOn       = rp ("dlyOn");
    par.dlySync     = rp ("dlySync");
    par.dlyTime     = rp ("dlyTime");
    par.dlyDiv      = rp ("dlyDiv");
    par.dlyFeedback = rp ("dlyFeedback");
    par.dlyMix      = rp ("dlyMix");

    par.fxOn      = rp ("fxOn");
    par.fxDist    = rp ("fxDist");
    par.fxSat     = rp ("fxSat");
    par.fxShape   = rp ("fxShape");
    par.fxRevSize = rp ("fxRevSize");
    par.fxRevMix  = rp ("fxRevMix");

    for (int o = 0; o < 3; ++o)
    {
        const juce::String prefix = "osc" + juce::String (o + 1);
        par.osc[o].waveform    = rp (prefix + "waveform");
        par.osc[o].detune      = rp (prefix + "detune");
        par.osc[o].mix         = rp (prefix + "mix");
        par.osc[o].stretch     = rp (prefix + "stretch");
        par.osc[o].stretchMode = rp (prefix + "stretchmode");
        par.osc[o].goldInt     = rp (prefix + "goldint");
        par.osc[o].tilt        = rp (prefix + "tilt");
    }

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

    // MIDI Learn: timer na wątku komunikatów drenuje FIFO eventów CC
    // i mapuje je na parametry (działa też bez otwartego edytora).
    startTimerHz (60);
}

FiSynthAudioProcessor::~FiSynthAudioProcessor()
{
    stopTimer();
}

double FiSynthAudioProcessor::getTailLengthSeconds() const
{
    // Szacunek z bieżących parametrów. Delay: pełne RT60 przy fb→0.9 to minuty,
    // więc kompromis 2+20·fb s (ogon poniżej ~-30 dB ginie w mikście). Pogłos:
    // freeverb przy size→1 ma RT60 ~10-12 s.
    double tail = 2.0;

    if (par.dlyOn != nullptr && par.dlyOn->load() > 0.5f)
        tail = juce::jmax (tail, 2.0 + 20.0 * (double) par.dlyFeedback->load());

    if (par.fxOn != nullptr && par.fxOn->load() > 0.5f && par.fxRevMix->load() > 0.001f)
        tail = juce::jmax (tail, 3.0 + 12.0 * (double) par.fxRevSize->load());

    return tail;
}

void FiSynthAudioProcessor::commitEnvelope (int idx)
{
    idx = juce::jlimit (0, kNumEnvelopes - 1, idx);
    {
        const juce::SpinLock::ScopedLockType sl (envLock);
        sharedSnapshots[idx] = envModels[idx].makeSnapshot();
    }
    envDirty.store (true);
    envEditCount.fetch_add (1);
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
        juce::ParameterID { "envGrid", 1 }, "Env Grid", divisionLabels(), 2));

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

        // Kolejność = enum FiSynthVoice::ModDest; nowe cele TYLKO na końcu
        // (APVTS serializuje wartość plain, więc stare presety zostają ważne).
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "Dest", 1 },
            "Env Dest " + juce::String (slot + 1),
            juce::StringArray { "None", "Filter Cutoff", "Pitch", "Osc Mix", "Resonance",
                                "Stretch 1", "Stretch 2", "Stretch 3",
                                "FM", "Tilt 1", "Tilt 2", "Tilt 3" },
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

        // 0..5 klasyczne, 6..12 rodzina φ/masek — kolejność musi zgadzać się
        // z fiHarmonicAmplitude; nowe pozycje TYLKO na końcu (stare presety).
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "waveform", 1 },
            prefix + " Waveform",
            juce::StringArray { "Sine", "Square", "Triangle", "Sawtooth", "Quadratic", "Noise",
                                "Pulse 25%", "Drawbar", "Even", "Golden Pluck",
                                "Fib Comb", "Lucas Comb", "Fib Word" },
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

        // Cel stretcha (rozkład częstotliwości partialek, do którego lerpuje suwak):
        //   0 Golden        = f·φ^n            (w pełni inharmoniczny, rozciągnięty)
        //   1 Fibonacci     = f·{1,2,3,5,8...} (niskie partialki tonalne, górne inharm.)
        //   2 Golden Octave = f·φ^log2(n+1)    (oktawa->φ; skompresowany, miękki)
        //   3 Golden Detune = harmoniczne mikro-rozstrojone kątem złotym (szerokość/chór)
        //   4/5 Silver/Bronze = f·σ^n          (metaliczne średnie; uogólnienie φ, ostrzejsze)
        //   6 Golden Stiff  = struna z sztywnością, wykładnik=φ (fortepianowe)
        //   7 Lucas         = f·Lucas(n)       (siostra Fibonacciego)
        //   8 Golden Shift  = harmoniczne przesunięte addytywnie o f/φ (klangor)
        // Parametr jest CIĄGŁY: część ułamkowa crossfade'uje między sąsiednimi
        // celami (morph z XY pada). ComboBox w GUI dalej działa przez ten sam
        // parametr — wybór z listy ustawia wartość całkowitą. Stare presety
        // (indeks 0..8 z dawnego AudioParameterChoice) wczytują się 1:1.
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "stretchmode", 1 },
            prefix + " Stretch Mode",
            juce::NormalisableRange<float> { 0.0f, (float) (fiNumStretchModes - 1), 0.001f },
            0.0f));

        // Gruby "złoty interwał": krotność 833.09¢ (=1200·log2 φ) dokładana do
        // wysokości oscylatora. Pozwala budować akordy w interwałach φ na 3 oscylatorach.
        // Indeks 0..4 -> k = indeks-2 (czyli -2φ, -φ, Off, +φ, +2φ).
        // Nazwy przez String(CharPointer_UTF8), nie gołą initializer-listę:
        // mieszana lista degraduje się do const char* i nie-ASCII literały
        // wpadają w asercję ASCII w String (juce_String.cpp:327).
        juce::StringArray goldIntChoices;
        goldIntChoices.add (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92""2\xcf\x86")));  // −2φ
        goldIntChoices.add (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92\xcf\x86")));      // −φ
        goldIntChoices.add ("Off");
        goldIntChoices.add (juce::String (juce::CharPointer_UTF8 ("+\xcf\x86")));                 // +φ
        goldIntChoices.add (juce::String (juce::CharPointer_UTF8 ("+2\xcf\x86")));                // +2φ

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { prefix + "goldint", 1 },
            prefix + " Golden Interval",
            goldIntChoices,
            2));

        // Golden Tilt: wykładnik opadania amplitud partiali (aₙ·(n+1)^(1−tilt)).
        // 1.0 = naturalne widmo waveformu; φ≈1.618 przyciemnia po złotym
        // wykładniku; <1 rozjaśnia. Skew tak, by 1.0 leżało blisko środka gałki.
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + "tilt", 1 },
            prefix + " Golden Tilt",
            juce::NormalisableRange<float> { 0.25f, 3.0f, 0.001f, 0.55f },
            1.0f));
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

    // Golden S&H: schodki ciągu Weyla frac(k/φ) — "random", który nigdy się
    // nie powtarza i równomiernie pokrywa zakres (low-discrepancy).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lfoShape", 1 },
        "LFO Shape",
        juce::StringArray { "Sine", "Square", "Triangle", "Golden S&H" },
        0));

    // Golden Drift: miks z drugim LFO o rate·φ — stosunek niewymierny, suma
    // nigdy się nie powtarza (quasi-periodyczny ruch jak drift analogu).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lfoDrift", 1 },
        juce::String (juce::CharPointer_UTF8 ("LFO Golden Drift (\xcf\x86)")),
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
        0.0f));

    // LFO sync do tempa: wł. = rate liczone z podziału nut (poniżej), wył. = Hz.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "lfoSync", 1 }, "LFO Tempo Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lfoRateDiv", 1 }, "LFO Rate (sync)",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T" }, 2));

    // === Gate Fibonacciego (trance-gate na wyjściu, pattern = słowo Fibonacciego) ===
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gateOn", 1 }, "Fib Gate On", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateDepth", 1 }, "Fib Gate Depth",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.85f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gateDiv", 1 }, "Fib Gate Step", divisionLabels(), 2));

    // Generacja słowa: kolejne podstawienia wydłużają pattern po Fibonaccim.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gateGen", 1 }, "Fib Gate Length",
        juce::StringArray { "5", "8", "13", "21", "34" }, 2));

    // Szerokość stereo phyllotaxis: pan partiali pod kątem złotym.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "spread", 1 }, "Stereo Spread",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // === Złoty silnik ===
    // Ring: dry/wet produktu osc1·osc2 (sidebandy f1±f2 wszystkich par partiali).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ringMix", 1 }, "Golden Ring",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // FM: sinus f·φ moduluje przyrosty faz wszystkich partiali (indeks 0..1).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fmAmt", 1 }, "Golden FM",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // Sub-partial na f/φ (złota "sub-oktawa": −833.09¢ pod fundamentem).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "subLevel", 1 }, "Golden Sub",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // Unison: bliźniaki dolnych partiali rozstrojone frac(n·φ) (low-discrepancy
    // detune — chór bez okresowego phasingu), pan odbity L↔R.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "unison", 1 }, "Golden Unison",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // Kwantyzacja modulacji Pitch do krotności złotego interwału 833.09¢.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "pitchQuant", 1 },
        juce::String (juce::CharPointer_UTF8 ("Pitch Quant 833\xc2\xa2")), false));

    // === Arpeggiator Fibonacciego ===
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "arpOn", 1 }, "Fib Arp On", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "arpDiv", 1 }, "Fib Arp Step", divisionLabels(), 2));

    // Długość cyklu melodii — pełny ciąg Fibonacciego (1 = repeater roota,
    // 2 = tryl, 3 = bieg chromatyczny, dalej klasyczne biegi złote).
    // UWAGA: lista przebudowana (dawniej {5,8,13}) — presety fabryczne
    // zregenerowane pod nowe indeksy.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "arpLen", 1 }, "Fib Arp Length",
        juce::StringArray { "1", "2", "3", "5", "8", "13" }, 4));

    // Tryb przebiegu po cyklu interwałów: In/Ex = czy skrajne kroki powtarzają
    // się na nawrocie; Random φ = schodki Weyla frac(k/φ) — "losowość", która
    // nigdy się nie zapętla i jest deterministyczna względem pozycji taktu.
    {
        juce::StringArray arpModes { "Up", "Down", "Up-Down In", "Up-Down Ex" };
        arpModes.add (juce::String (juce::CharPointer_UTF8 ("Random \xcf\x86")));
        arpModes.add ("Random");
        arpModes.add ("Fib Walk");   // pozycja skacze o kolejne liczby Fibonacciego
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "arpMode", 1 }, "Fib Arp Mode", arpModes, 0));
    }

    // Rytm ze słowa Fibonacciego: S = 1 podział, L = 2 podziały (13 nut na
    // 18 podziałów) — aperiodyczny groove zamiast równych kroków.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "arpWord", 1 }, "Fib Arp Word Rhythm", false));

    // Humanizacja velocity schodkami Weyla frac(k·φ): deterministyczna
    // "ludzka" dynamika, która nigdy się nie zapętla.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "arpVel", 1 },
        juce::String (juce::CharPointer_UTF8 ("Fib Arp Velocity \xcf\x86")),
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // === Golden Delay ===
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "dlyOn", 1 }, "Golden Delay On", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "dlySync", 1 }, "Golden Delay Sync", true));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dlyTime", 1 }, "Golden Delay Time",
        juce::NormalisableRange<float> { 30.0f, 1500.0f, 1.0f, 0.5f }, 400.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "dlyDiv", 1 }, "Golden Delay Time (sync)",
        divisionLabels(), 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dlyFeedback", 1 }, "Golden Delay Feedback",
        juce::NormalisableRange<float> { 0.0f, 0.9f, 0.01f }, 0.45f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dlyMix", 1 }, "Golden Delay Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.25f));

    // === Efektor (distortion / saturation / waveshaper / reverb) ===
    // Każdy stopień drive'u miksowany dry/wet swoim amountem — 0 = neutralnie,
    // a głośność nie ucieka z gainem.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "fxOn", 1 }, "FX On", false));

    // Hard clip z gainem do ×10 (agresywny przester).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fxDist", 1 }, "FX Distortion",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // Nasycenie tanh (ciepły, lampowy soft clip).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fxSat", 1 }, "FX Saturation",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    // Waveshaper: folder sinusoidalny, drive rośnie do 1+2φ (fałdowanie fali).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fxShape", 1 }, "FX Waveshaper",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fxRevSize", 1 }, "FX Reverb Size",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fxRevMix", 1 }, "FX Reverb Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.0f));

    return { params.begin(), params.end() };
}

juce::StringArray FiSynthAudioProcessor::divisionLabels()
{
    juce::StringArray labels;
    for (const auto& d : divisions)
        labels.add (d.label);
    return labels;
}

// Słowo Fibonacciego przez podstawienia S→SL, L→S; genIdx wybiera długość
// 5/8/13/21/34 (kolejne liczby Fibonacciego). Bit i = 1 gdy krok i to 'S'.
FiSynthAudioProcessor::FibWord FiSynthAudioProcessor::fibonacciWord (int genIdx)
{
    static const auto table = []
    {
        std::array<FibWord, 5> t {};
        std::string w = "S";
        int idx = 0;

        for (int gen = 0; gen < 12 && idx < 5; ++gen)
        {
            std::string next;
            for (const char c : w)
                next += (c == 'S') ? "SL" : "S";
            w = std::move (next);

            static constexpr int wanted[] = { 5, 8, 13, 21, 34 };
            if ((int) w.length() == wanted[idx])
            {
                juce::uint64 bits = 0;
                for (size_t i = 0; i < w.length(); ++i)
                    if (w[i] == 'S')
                        bits |= (juce::uint64) 1 << i;
                t[(size_t) idx] = { bits, (int) w.length() };
                ++idx;
            }
        }
        return t;
    }();

    return table[(size_t) juce::jlimit (0, 4, genIdx)];
}

void FiSynthAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    // Golden Delay: 2.2 s bufora (maks. czas 1.5 s / 1 beat przy wolnym tempie,
    // z zapasem na interpolację).
    delayBuffer.setSize (2, (int) (sampleRate * 2.2) + 16);
    delayBuffer.clear();
    delayWritePos = 0;
    delaySmoothSamples = -1.0f;

    // Bufor filtra MIDI arpa: pojemność z zapasem TERAZ, żeby processArp
    // nie alokował na wątku audio.
    arpFiltered.ensureSize (4096);

    // Efektor: pogłos + stan wygładzania drive'u.
    fxReverb.setSampleRate (sampleRate);
    fxReverb.reset();
    fxRevTailSamples = 0;
    fxDistEnv = fxSatEnv = fxShapeEnv = 0.0f;

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

    // MIDI Learn: eventy CC do lock-free FIFO — mapowanie na parametry robi
    // timerCallback na wątku komunikatów (setValueNotifyingHost nie może być
    // wołane z audio). Eventy zostają też w buforze (synth i tak ignoruje
    // wszystko poza sustain/panic). Trzy pułapki obchodzone celowo:
    //   • surowe bajty zamiast meta.getMessage() — MidiMessage > 8 bajtów
    //     (SysEx) malloc'owałby na wątku audio,
    //   • CC >= 120 (channel mode: panic/reset, DAW śle je same przy stopie
    //     transportu) nie są "gałkami" — ani do Learn, ani do mapy,
    //   • gdy mapa pusta i Learn nieuzbrojony (ccActive), pętla w ogóle nie rusza.
    if (ccActive.load (std::memory_order_relaxed))
    {
        for (const auto meta : midiMessages)
        {
            if (meta.numBytes != 3 || (meta.data[0] & 0xf0) != 0xb0
                || meta.data[1] >= 120)
                continue;

            const auto scope = ccFifo.write (1);
            if (scope.blockSize1 == 1)
                ccEvents[scope.startIndex1] = { (int) meta.data[1],
                                                (float) meta.data[2] / 127.0f };
        }
    }

    // Nuty z klawiatury ekranowej dołączają do strumienia MIDI (i odwrotnie:
    // eventy z hosta aktualizują stan klawiszy w GUI).
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

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
    const bool tempoSync = par.tempoSync->load() > 0.5f;

    double hostBpm = 0.0;
    if (tempoSync)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    hostBpm = *b;

    const float manualBpm = par.bpm->load();
    const float bpm = hostBpm > 0.0 ? (float) hostBpm : manualBpm;
    currentBpm.store (bpm);

    // Wspólny zegar beatowy: pozycja startu bloku (ppq hosta gdy transport
    // gra, inaczej wolnobieżna) liczona RAZ dla gate'a i arpa — jeden czas,
    // patterny zawsze zazębione.
    double blockStartBeat = beatPos;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    blockStartBeat = *ppq;

    const double beatsPerSample = (double) juce::jmax (1.0f, bpm) / 60.0 / getSampleRate();
    beatPos = blockStartBeat + beatsPerSample * buffer.getNumSamples();

    // Arpeggiator Fibonacciego: przy włączonym arp trzymane klawisze tylko
    // wybierają root, a nuty generuje zegar krokowy (przed renderem syntezatora).
    processArp (midiMessages, buffer.getNumSamples(), blockStartBeat, beatsPerSample);

    const bool   envSync   = par.envSync->load() > 0.5f;
    const double timeScale = envSync ? (60.0 / juce::jmax (1.0f, bpm)) : 1.0;

    // Routing modulacji obwiedni (3 sloty).
    int   envDest[3];
    float envAmt[3];
    for (int slot = 0; slot < 3; ++slot)
    {
        envDest[slot] = (int) par.envDest[slot]->load();
        envAmt[slot]  = par.envAmt[slot]->load();
    }

    // Odczyt parametrów.
    float filterCutoff = par.filterCutoff->load();
    float filterResonance = par.filterResonance->load();
    int filterType = (int) par.filterType->load();

    // LFO rate: w trybie sync liczone z podziału nut pod bieżące BPM
    // (rate[Hz] = (BPM/60) / beaty-na-cykl); inaczej z ręcznego suwaka w Hz.
    const bool lfoSync = par.lfoSync->load() > 0.5f;
    float lfoRate;
    if (lfoSync)
    {
        float lfoBeats = 1.0f;
        switch ((int) par.lfoRateDiv->load())
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
        lfoRate = par.lfoRate->load();

    float lfoDepth = par.lfoDepth->load();
    int lfoShape = (int) par.lfoShape->load();
    const float lfoDrift = par.lfoDrift->load();

    // Złoty silnik (wspólny dla wszystkich głosów).
    const float ringMix    = par.ringMix->load();
    const float fmAmt      = par.fmAmt->load();
    const float subLevel   = par.subLevel->load();
    const float unison     = par.unison->load();
    const bool  pitchQuant = par.pitchQuant->load() > 0.5f;

    // Parametry oscylatorów raz na blok — wspólne dla wszystkich głosów.
    int   oscWaveform[3];
    float oscDetune[3], oscMix[3], oscStretch[3], oscStretchMode[3], oscCoarse[3], oscTilt[3];
    for (int o = 0; o < 3; ++o)
    {
        oscWaveform[o]    = (int) par.osc[o].waveform->load();
        oscDetune[o]      = par.osc[o].detune->load();
        oscMix[o]         = par.osc[o].mix->load();
        oscStretch[o]     = par.osc[o].stretch->load();
        oscStretchMode[o] = par.osc[o].stretchMode->load();   // ciągłe 0..8 (morph)
        oscCoarse[o]      = fiGoldIntCents ((int) par.osc[o].goldInt->load());
        oscTilt[o]        = par.osc[o].tilt->load();
    }

    const float spread = par.spread->load();

    // Synchronizuj na wszystkie głosy.
    for (auto* voice : fiVoices)
    {
        for (int e = 0; e < kNumEnvelopes; ++e)
            voice->setEnvelope (e, &audioSnapshots[e]);
        voice->setEnvTimeScale (timeScale);
        voice->setFilterParams (filterCutoff, filterResonance, filterType);
        voice->setLFOParams (lfoRate, lfoDepth, lfoShape, lfoDrift);
        voice->setStereoSpread (spread);
        voice->setGoldenParams (fmAmt, ringMix, subLevel, unison, pitchQuant);

        for (int slot = 0; slot < 3; ++slot)
            voice->setEnvMod (slot, envDest[slot], envAmt[slot]);

        for (int o = 0; o < 3; ++o)
            voice->setOscillatorParams (o, oscWaveform[o], oscDetune[o], oscMix[o],
                                        oscStretch[o], oscStretchMode[o], oscCoarse[o],
                                        oscTilt[o]);
    }

    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Playhead: zbierz do kMaxPlayheads aktywnych głosów, każdy jako osobna kreska
    // (czas bezwzględny + poziom), osobno dla każdej obwiedni.
    {
        int slot = 0;
        for (auto* voice : fiVoices)
        {
            if (slot >= kMaxPlayheads)
                break;

            if (voice->isEnvActive())
            {
                for (int e = 0; e < kNumEnvelopes; ++e)
                    envPlayheadTime[e][slot].store (voice->getEnvTime (e));

                envPlayheadNote[slot].store (voice->getCurrentlyPlayingNote());
                ++slot;
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

    // Gate Fibonacciego przed master gainem; FIFO widma bierze to, co słychać.
    applyFibGate (buffer, blockStartBeat, beatsPerSample);

    // Efektor, część drive (dist → sat → fold): za gate'em, przed delayem —
    // echa niosą już przester, feedback nie przesterowuje się kumulacyjnie.
    applyFxDrive (buffer);

    // Golden Delay za gate'em (bramka tnie sygnał, delay rozmywa echa —
    // odwrotna kolejność zjadałaby ogony taktem bramki).
    applyGoldenDelay (buffer, bpm);

    // Pogłos na końcu łańcucha: ogona nie tnie gate ani nie klipuje drive.
    applyFxReverb (buffer);

    const float gain = par.gain->load();
    buffer.applyGainRamp (0, buffer.getNumSamples(), previousGain, gain);
    previousGain = gain;

    pushAnalyzerSamples (buffer);

    // W buforze bywają nasze własne eventy (arp, klawiatura ekranowa), a plugin
    // nie deklaruje wyjścia MIDI — wrapper VST3 i tak je wyrzuca, ale w buildzie
    // Debug pilnuje tego asercją (jassert "więcej eventów niż weszło"), która
    // pod hostem zatrzymuje proces, gdy tylko arp doda pierwszą nutę.
    midiMessages.clear();
}

void FiSynthAudioProcessor::applyFibGate (juce::AudioBuffer<float>& buffer,
                                          double blockStartBeat, double beatsPerSample)
{
    const bool on = par.gateOn->load() > 0.5f;

    // Po wyłączeniu dogrywamy ramp do 1.0, żeby nie zostawić skoku głośności.
    if (! on && gateEnv > 0.999f)
    {
        gateStep.store (-1);
        return;
    }

    const auto  word      = fibonacciWord ((int) par.gateGen->load());
    const float depth     = par.gateDepth->load();
    const auto  flips     = gateFlips.load();
    const float stepBeats = divisionToBeats ((int) par.gateDiv->load());

    // Pozycja w beatach ze wspólnego zegara (patrz beatPos w processBlock).
    double beat = blockStartBeat;

    // One-pole ~1.5 ms — bramka bez trzasków na krawędziach kroków.
    const float smooth = std::exp (-1.0f / (0.0015f * (float) getSampleRate()));

    auto* const* chans = buffer.getArrayOfWritePointers();
    const int numCh    = buffer.getNumChannels();
    const int numSmp   = buffer.getNumSamples();

    int lastStep = -1;
    for (int i = 0; i < numSmp; ++i)
    {
        // Floor-mod: ppq bywa UJEMNE (count-in/pre-roll hosta) — zwykłe
        // (int)/% dałoby ujemny krok i shift uint64 o ujemną liczbę (UB).
        const int raw  = (int) std::floor (beat / (double) stepBeats);
        const int step = ((raw % word.len) + word.len) % word.len;

        const bool open = (((word.bits ^ flips) >> step) & 1) != 0;
        const float target = (on && ! open) ? 1.0f - depth : 1.0f;

        gateEnv = target + (gateEnv - target) * smooth;
        for (int ch = 0; ch < numCh; ++ch)
            chans[ch][i] *= gateEnv;

        beat += beatsPerSample;
        lastStep = step;
    }

    // Pusty blok nie ma prawa zgasić wskaźnika kroku w GUI.
    if (numSmp > 0)
        gateStep.store (on ? lastStep : -1);
}

void FiSynthAudioProcessor::processArp (juce::MidiBuffer& midi, int numSamples,
                                        double blockStartBeat, double beatsPerSample)
{
    const bool on = par.arpOn->load() > 0.5f;

    if (! on)
    {
        // Wyłączenie: puść ostatnią nutę arp i zapomnij trzymane klawisze
        // (klawisze wciśnięte przy wyłączonym arp śledzi normalny tor MIDI).
        if (arpNoteSounding >= 0)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, arpNoteSounding), 0);
            arpNoteSounding = -1;
        }
        if (arpWasOn)
            std::fill (std::begin (arpHeld), std::end (arpHeld), false);
        arpWasOn = false;
        return;
    }

    // Note on/off aktualizują TYLKO zbiór trzymanych klawiszy; nie docierają do
    // syntezatora. Pozostałe eventy (pitch bend, CC, sustain) przechodzą dalej.
    // Panic (CC 123/120) musi też czyścić trzymane klawisze — inaczej arp
    // odpaliłby nuty od nowa zaraz po wyciszeniu.
    arpFiltered.clear();
    for (const auto meta : midi)
    {
        // Surowe bajty zamiast meta.getMessage() — SysEx (>8 bajtów) robiłby
        // malloc na wątku audio (ten sam powód co w pętli CC w processBlock).
        const auto* d      = meta.data;
        const int   status = meta.numBytes >= 1 ? (d[0] & 0xf0) : 0;
        const bool  note   = meta.numBytes == 3 && (status == 0x80 || status == 0x90);

        if (note)
            // Note-on z velocity 0 to note-off (konwencja MIDI).
            arpHeld[d[1] & 0x7f] = (status == 0x90 && d[2] != 0);
        else
        {
            if (meta.numBytes == 3 && status == 0xb0 && (d[1] == 120 || d[1] == 123))
                std::fill (std::begin (arpHeld), std::end (arpHeld), false);
            arpFiltered.addEvent (d, meta.numBytes, meta.samplePosition);
        }
    }
    midi.swapWith (arpFiltered);

    // Włączenie w locie: zdejmij nuty grające "normalnie" (CC 123), inaczej
    // wisiałyby bez note-offów, które od teraz zjada filtr powyżej.
    if (! arpWasOn)
    {
        midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        arpWasOn = true;
        arpLastStep = INT_MIN;
    }

    // Root = najniższy trzymany klawisz (kwantowane do bloku — wystarcza).
    int root = -1;
    for (int n = 0; n < 128; ++n)
        if (arpHeld[n]) { root = n; break; }

    if (root < 0 && arpNoteSounding >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (1, arpNoteSounding), 0);
        arpNoteSounding = -1;
    }

    // Melodia: interwały Fibonacciego w półtonach od roota (0,1,2,3,5,8,13,21),
    // dalsze wyrazy składane mod 24, żeby nie uciec z rejestru.
    static constexpr int fibOffsets[13] = { 0, 1, 2, 3, 5, 8, 13, 21, 10, 7, 17, 0, 17 };
    static constexpr int lens[6] = { 1, 2, 3, 5, 8, 13 };
    const int lenIdx = juce::jlimit (0, 5, (int) par.arpLen->load());
    const int len    = lens[lenIdx];
    const int mode   = juce::jlimit (0, 6, (int) par.arpMode->load());

    // Numer kroku (może być ujemny w pre-rollu) -> indeks w cyklu interwałów.
    // In/Ex: czy skrajne kroki powtarzają się na nawrocie ping-ponga.
    // Random φ liczy z ABSOLUTNEGO kroku (schodki Weyla) — pattern jest
    // deterministyczny względem pozycji taktu, więc loop w DAW gra identycznie.
    auto posmod = [] (int a, int m) noexcept { return ((a % m) + m) % m; };
    auto offsetIndexFor = [&] (int step) noexcept
    {
        switch (mode)
        {
            case 1:  // Down
                return len - 1 - posmod (step, len);
            case 2:  // Up-Down In (skraje powtórzone: 0..L-1, L-1..0)
            {
                const int p = posmod (step, 2 * len);
                return p < len ? p : 2 * len - 1 - p;
            }
            case 3:  // Up-Down Ex (skraje pojedynczo: 0..L-1, L-2..1)
            {
                const int period = juce::jmax (1, 2 * len - 2);
                const int p = posmod (step, period);
                return p < len ? p : 2 * len - 2 - p;
            }
            case 4:  // Random φ — ciąg Weyla frac(k/φ): równomierny, nieokresowy
            {
                const double g = (double) step / fiPhi;
                return juce::jmin (len - 1, (int) ((g - std::floor (g)) * len));
            }
            case 5:  // Random — prawdziwy los (per krok)
                return arpRng.nextInt (juce::jmax (1, len));
            case 6:  // Fib Walk — pozycja = (F(k+2)−1) mod len: cykl przemierzany
            {        // skokami o kolejne liczby Fibonacciego. F mod len jest
                     // okresowe (okres Pisano), więc liczymy z małego k —
                     // deterministycznie względem pozycji taktu.
                static constexpr int pisano[6] = { 1, 3, 8, 20, 12, 28 };  // dla len 1,2,3,5,8,13
                const int j = posmod (step, pisano[lenIdx]);
                int a = 1 % len, b = 1 % len;              // F(1), F(2) mod len
                for (int i = 0; i < j; ++i)
                {
                    const int c = (a + b) % len;
                    a = b; b = c;
                }
                return posmod (b - 1, len);                // b = F(j+2) mod len
            }
            default: // Up
                return posmod (step, len);
        }
    };

    const float stepBeats = divisionToBeats ((int) par.arpDiv->load());

    // Mapowanie pozycji beatowej -> numer NUTY. Przy równym rytmie nuta = numer
    // podziału; przy rytmie ze słowa Fibonacciego S trwa 1 podział, a L dwa
    // (SLSSLSLSSLSSL: 13 nut na 18 podziałów) — tabela podział->nuta poniżej.
    const bool wordRhythm = par.arpWord->load() > 0.5f;
    static constexpr int kWordNotes = 13, kWordUnits = 18;
    static constexpr int wordNoteAtUnit[kWordUnits] =
        { 0, 1, 1, 2, 3, 4, 4, 5, 6, 6, 7, 8, 9, 9, 10, 11, 12, 12 };
    auto noteIndexAtBeat = [&] (double b) noexcept
    {
        const int unit = (int) std::floor (b / (double) stepBeats);
        if (! wordRhythm)
            return unit;
        const int cyc = (int) std::floor ((double) unit / (double) kWordUnits);
        return cyc * kWordNotes + wordNoteAtUnit[unit - cyc * kWordUnits];
    };

    // Velocity φ: schodki Weyla frac(k·φ) odejmowane od bazy — deterministyczna
    // "ludzka" dynamika (do −60 przy pełnej głębokości), stabilna w loopie DAW.
    const float velDepth = par.arpVel->load();
    auto velocityFor = [&] (int noteIdx) noexcept
    {
        if (velDepth <= 0.001f)
            return (juce::uint8) 100;
        const double g = (double) noteIdx / fiPhi;
        const double w = g - std::floor (g);
        return (juce::uint8) juce::jlimit (30, 127,
                   (int) std::lround (100.0 - velDepth * w * 60.0));
    };

    // Pozycja w beatach ze wspólnego zegara (ten sam co gate — patrz beatPos).
    double beat = blockStartBeat;

    for (int i = 0; i < numSamples; ++i)
    {
        // Numer nuty z pozycji beatowej (floor: ppq bywa ujemne w pre-rollu).
        const int raw = noteIndexAtBeat (beat);
        if (raw != arpLastStep)
        {
            arpLastStep = raw;

            if (arpNoteSounding >= 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, arpNoteSounding), i);
                arpNoteSounding = -1;
            }

            if (root >= 0)
            {
                const int idx  = offsetIndexFor (raw);
                const int note = juce::jmin (127, root + fibOffsets[idx]);
                midi.addEvent (juce::MidiMessage::noteOn (1, note, velocityFor (raw)), i);
                arpNoteSounding = note;
            }
        }
        beat += beatsPerSample;
    }
}

void FiSynthAudioProcessor::applyGoldenDelay (juce::AudioBuffer<float>& buffer, float bpm)
{
    const bool on = par.dlyOn->load() > 0.5f;
    if (! on)
    {
        // Czyścimy linię przy wyłączeniu — ponowne włączenie startuje z ciszy,
        // a nie ze stęchłego ogona sprzed minuty.
        if (dlyWasOn)
        {
            delayBuffer.clear();
            delaySmoothSamples = -1.0f;
        }
        dlyWasOn = false;
        return;
    }
    dlyWasOn = true;

    const int bufLen = delayBuffer.getNumSamples();
    if (bufLen == 0)
        return;

    const double sr = getSampleRate();
    const float timeMs = par.dlySync->load() > 0.5f
        ? divisionToBeats ((int) par.dlyDiv->load()) * 60000.0f / juce::jmax (1.0f, bpm)
        : par.dlyTime->load();

    const float target = juce::jlimit (32.0f, (float) (bufLen - 8),
                                       (float) (timeMs * 0.001 * sr));
    if (delaySmoothSamples < 0.0f)
        delaySmoothSamples = target;

    const float fb  = par.dlyFeedback->load();
    const float wet = par.dlyMix->load();

    // ~80 ms wygładzania czasu — kręcenie gałką daje "taśmowy" glide, nie zgrzyt.
    const float smooth = std::exp (-1.0f / (0.08f * (float) sr));

    // Tapy w czasach t/φ³..t: odstępy złote (wzajemnie niewspółmierne), więc
    // odbicia nie kumulują się w okresowy grzebień. Wzmocnienia opadają po 1/φ
    // w stronę wcześniejszych tapów (pre-echa cichsze od głównego).
    constexpr float invPhi = (float) (1.0 / fiPhi);
    constexpr float tapFrac[4] = { invPhi * invPhi * invPhi, invPhi * invPhi, invPhi, 1.0f };
    constexpr float tapAmp [4] = { 0.13f, 0.21f, 0.34f, 0.55f };

    auto* dL = delayBuffer.getWritePointer (0);
    auto* dR = delayBuffer.getWritePointer (1);

    const int numCh  = buffer.getNumChannels();
    const int numSmp = buffer.getNumSamples();
    auto* inL = buffer.getWritePointer (0);
    auto* inR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    auto readTap = [bufLen] (const float* line, float delaySamp, int wp) noexcept
    {
        float pos = (float) wp - delaySamp;
        if (pos < 0.0f)
            pos += (float) bufLen;
        // Zaokrąglenie float przy pos bliskim zera może dać dokładnie bufLen
        // po dodaniu — bez tego domknięcia i0 czytałby 1 element za buforem.
        if (pos >= (float) bufLen)
            pos -= (float) bufLen;
        const int   i0 = (int) pos;
        const float f  = pos - (float) i0;
        const int   i1 = (i0 + 1 < bufLen) ? i0 + 1 : 0;
        return line[i0] + (line[i1] - line[i0]) * f;
    };

    int   wp = delayWritePos;
    float t  = delaySmoothSamples;

    for (int i = 0; i < numSmp; ++i)
    {
        t = target + (t - target) * smooth;

        const float xL = inL[i];
        const float xR = inR != nullptr ? inR[i] : xL;

        float wetL = 0.0f, wetR = 0.0f, mainL = 0.0f, mainR = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            const float rl = readTap (dL, t * tapFrac[k], wp);
            const float rr = readTap (dR, t * tapFrac[k], wp);
            wetL += tapAmp[k] * rl;
            wetR += tapAmp[k] * rr;
            if (k == 3) { mainL = rl; mainR = rr; }
        }

        // Sprzężenie na krzyż z tapu pełnego czasu — echa wędrują L↔R (ping-pong).
        dL[wp] = xL + fb * mainR;
        dR[wp] = xR + fb * mainL;

        inL[i] = xL + wet * wetL;
        if (inR != nullptr)
            inR[i] = xR + wet * wetR;

        if (++wp >= bufLen)
            wp = 0;
    }

    delayWritePos = wp;
    delaySmoothSamples = t;
}

void FiSynthAudioProcessor::applyFxDrive (juce::AudioBuffer<float>& buffer)
{
    // Cele amountów: 0 gdy sekcja wyłączona — wyłączenie to zjazd wygładzanych
    // envów do zera, nie skokowa podmiana kształtu fali (klik przy fxOn off
    // z rozgrzanym dist=1: ±0.8 clip → surowy sygnał w jednej próbce).
    const bool on = par.fxOn->load() > 0.5f;
    const float distT  = on ? par.fxDist->load()  : 0.0f;
    const float satT   = on ? par.fxSat->load()   : 0.0f;
    const float shapeT = on ? par.fxShape->load() : 0.0f;

    constexpr float eps = 1.0e-4f;
    if (distT < eps && satT < eps && shapeT < eps
        && fxDistEnv < eps && fxSatEnv < eps && fxShapeEnv < eps)
    {
        fxDistEnv = fxSatEnv = fxShapeEnv = 0.0f;
        return;
    }

    // One-pole ~5 ms na amountach; gainy stopni liczone z ENVA per próbka,
    // więc i automacja (block-quantized, także z MIDI Learn) idzie bez zipperu.
    // Każdy stopień miksowany dry/wet swoim amountem — małe wartości wchodzą
    // płynnie, a głośność zostaje w ryzach.
    const float smooth = std::exp (-1.0f / (0.005f * (float) getSampleRate()));

    const bool doDist  = distT  >= eps || fxDistEnv  >= eps;   // hard clip, do ×10
    const bool doSat   = satT   >= eps || fxSatEnv   >= eps;   // tanh
    const bool doShape = shapeT >= eps || fxShapeEnv >= eps;   // folder: >π/2 fałduje

    constexpr float foldBase = juce::MathConstants<float>::halfPi;
    const float     foldPhi  = 2.0f * (float) fiPhi;

    auto* const* chans = buffer.getArrayOfWritePointers();
    const int numCh  = buffer.getNumChannels();
    const int numSmp = buffer.getNumSamples();

    float dEnv = fxDistEnv, sEnv = fxSatEnv, fEnv = fxShapeEnv;
    for (int i = 0; i < numSmp; ++i)
    {
        dEnv = distT  + (dEnv - distT)  * smooth;
        sEnv = satT   + (sEnv - satT)   * smooth;
        fEnv = shapeT + (fEnv - shapeT) * smooth;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = chans[ch][i];

            if (doDist)
            {
                const float wet = juce::jlimit (-1.0f, 1.0f, x * (1.0f + 9.0f * dEnv)) * 0.8f;
                x += dEnv * (wet - x);
            }
            if (doSat)
                x += sEnv * (std::tanh (x * (1.0f + 5.0f * sEnv)) - x);
            if (doShape)
                x += fEnv * (std::sin (x * foldBase * (1.0f + foldPhi * fEnv)) - x);

            chans[ch][i] = x;
        }
    }
    fxDistEnv = dEnv;
    fxSatEnv  = sEnv;
    fxShapeEnv = fEnv;
}

void FiSynthAudioProcessor::applyFxReverb (juce::AudioBuffer<float>& buffer)
{
    const float mix    = par.fxRevMix->load();
    const bool  on     = par.fxOn->load() > 0.5f && mix > 0.001f;
    const int   numSmp = buffer.getNumSamples();

    // Wyłączenie nie tnie ogona między blokami: przez ~0.25 s gramy dalej
    // z celem wet=0 (wewnętrzne SmoothedValue Reverbu ściągają gain płynnie),
    // dopiero potem reset (ponowne włączenie startuje z ciszy, jak delay).
    if (! on && fxRevTailSamples <= 0)
        return;

    if (on)
        fxRevTailSamples = (int) (0.25 * getSampleRate());
    else
        fxRevTailSamples -= numSmp;

    juce::Reverb::Parameters rp;
    rp.roomSize = par.fxRevSize->load();
    rp.damping  = 0.5f;
    // Freeverb mnoży dry ×2 i wet ×3 — 0.5/0.45 daje jedność na dry (bez
    // skoku +6 dB przy włączaniu sekcji) przy dawnej proporcji wet:dry.
    rp.wetLevel = on ? mix * 0.45f : 0.0f;
    rp.dryLevel = 0.5f;
    rp.width    = 1.0f;
    fxReverb.setParameters (rp);

    if (buffer.getNumChannels() >= 2)
        fxReverb.processStereo (buffer.getWritePointer (0),
                                buffer.getWritePointer (1), numSmp);
    else
        fxReverb.processMono (buffer.getWritePointer (0), numSmp);

    if (! on && fxRevTailSamples <= 0)
        fxReverb.reset();
}

void FiSynthAudioProcessor::pushAnalyzerSamples (const juce::AudioBuffer<float>& buffer)
{
    const int numSmp = buffer.getNumSamples();
    const int numCh  = juce::jmax (1, buffer.getNumChannels());

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analyzerFifo.prepareToWrite (numSmp, start1, size1, start2, size2);

    auto writeRange = [&] (int start, int size, int offset)
    {
        for (int i = 0; i < size; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                s += buffer.getSample (ch, offset + i);
            analyzerBuffer[(size_t) (start + i)] = s / (float) numCh;
        }
    };
    writeRange (start1, size1, 0);
    writeRange (start2, size2, size1);

    analyzerFifo.finishedWrite (size1 + size2);
}

int FiSynthAudioProcessor::readAnalyzerSamples (float* dest, int maxNum)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    analyzerFifo.prepareToRead (maxNum, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        dest[i] = analyzerBuffer[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i)
        dest[size1 + i] = analyzerBuffer[(size_t) (start2 + i)];

    analyzerFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

// === MIDI Learn ===
// Wszystko poniżej bierze ccMapLock — patrz komentarz przy ccMap w nagłówku.

void FiSynthAudioProcessor::updateCcActive() noexcept
{
    bool any = (learnParam != nullptr);
    for (auto* p : ccMap)
        any = any || (p != nullptr);
    ccActive.store (any, std::memory_order_relaxed);
}

void FiSynthAudioProcessor::timerCallback()
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);

    // Koalescencja: nakręcona gałka to dziesiątki eventów na tick, a słychać
    // tylko ostatnią wartość — na parametr idzie JEDEN gest na tick (host
    // w trybie touch/latch widzi jedno dotknięcie, nie serię zerowych).
    float pending[128];
    bool  dirty[128] {};

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    ccFifo.prepareToRead (ccFifo.getNumReady(), start1, size1, start2, size2);

    auto gather = [&] (int idx)
    {
        const auto ev = ccEvents[idx];   // producent gwarantuje num 0..119

        if (learnParam != nullptr)
        {
            // Uzbrojony Learn: pierwszy CC przejmuje parametr (1:1 — stare
            // przypisanie parametru znika, przypisanie CC nadpisuje się).
            for (auto*& p : ccMap)
                if (p == learnParam)
                    p = nullptr;

            ccMap[ev.num] = learnParam;
            learnParam    = nullptr;
            updateCcActive();
        }

        pending[ev.num] = ev.val;
        dirty[ev.num]   = true;
    };

    for (int i = 0; i < size1; ++i) gather (start1 + i);
    for (int i = 0; i < size2; ++i) gather (start2 + i);
    ccFifo.finishedRead (size1 + size2);

    for (int cc = 0; cc < 128; ++cc)
        if (dirty[cc])
            if (auto* p = ccMap[cc])
            {
                // Para gestów jak dotknięcie gałki w GUI.
                p->beginChangeGesture();
                p->setValueNotifyingHost (pending[cc]);
                p->endChangeGesture();
            }
}

void FiSynthAudioProcessor::startMidiLearn (const juce::String& paramID)
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);
    learnParam = apvts.getParameter (paramID);
    updateCcActive();
}

void FiSynthAudioProcessor::cancelMidiLearn()
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);
    learnParam = nullptr;
    updateCcActive();
}

bool FiSynthAudioProcessor::isMidiLearnArmed (const juce::String& paramID) const
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);
    return learnParam != nullptr && learnParam->paramID == paramID;
}

int FiSynthAudioProcessor::ccForParam (const juce::String& paramID) const
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[cc] != nullptr && ccMap[cc]->paramID == paramID)
            return cc;
    return -1;
}

void FiSynthAudioProcessor::clearCcMapping (const juce::String& paramID)
{
    const juce::SpinLock::ScopedLockType lock (ccMapLock);
    for (auto*& p : ccMap)
        if (p != nullptr && p->paramID == paramID)
            p = nullptr;
    updateCcActive();
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

    // Flipy patternu gate'a (maska bitowa jako hex — ValueTree nie ma uint64).
    auto gateTree = apvts.state.getOrCreateChildWithName ("GATE", nullptr);
    gateTree.setProperty ("flips", juce::String::toHexString ((juce::int64) gateFlips.load()), nullptr);

    return apvts.copyState().createXml();
}

// Odtwarza pełny stan z XML (parametry + obwiednie). Współdzielone przez stan
// DAW i pliki presetów.
void FiSynthAudioProcessor::applyStateXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;

    auto newState = juce::ValueTree::fromXml (xml);

    // Starszy preset nie zna później dodanych parametrów, a replaceState
    // zostawia brakującym ich BIEŻĄCE wartości (updateParameterConnections...
    // flushuje aktualny stan, nie domyślny) — stary preset wczytany przy
    // podkręconym złotym silniku brzmiałby inaczej niż w dniu zapisu.
    // Defaulty dopisujemy do DRZEWA przed pojedynczym replaceState: wczytanie
    // jest atomowe (audio nigdy nie gra hybrydą starego i nowego patcha),
    // a host nie dostaje lawiny powiadomień o "ruchach" parametrów, których
    // nikt nie ruszał (tryb automation write nagrywałby je jako automację).
    juce::StringArray savedIds;
    for (const auto& child : newState)
        if (child.hasType ("PARAM"))
            savedIds.add (child.getProperty ("id").toString());

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! savedIds.contains (rp->paramID))
            {
                juce::ValueTree child ("PARAM");
                child.setProperty ("id", rp->paramID, nullptr);
                child.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
                newState.appendChild (child, nullptr);
            }

    apvts.replaceState (newState);

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

    // Flipy gate'a (brak w starym stanie => czysty pattern).
    const auto gateTree = apvts.state.getChildWithName ("GATE");
    gateFlips.store (gateTree.isValid()
        ? (juce::uint64) gateTree.getProperty ("flips").toString().getHexValue64()
        : 0);
}

void FiSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateToXml())
    {
        // Mapa MIDI CC tylko w stanie DAW/standalone, NIE w presetach (dokłada
        // ją ten szczebel, nie stateToXml): preset to brzmienie, mapa to sprzęt
        // użytkownika — wczytanie presetu nie może kasować mapy kontrolera.
        auto* map = xml->createNewChildElement ("MIDIMAP");
        {
            const juce::SpinLock::ScopedLockType lock (ccMapLock);
            for (int cc = 0; cc < 128; ++cc)
                if (ccMap[cc] != nullptr)
                {
                    auto* m = map->createNewChildElement ("MAP");
                    m->setAttribute ("cc", cc);
                    m->setAttribute ("param", ccMap[cc]->paramID);
                }
        }

        copyXmlToBinary (*xml, destData);
    }
}

void FiSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        // MIDIMAP wyjmij PRZED applyStateXml — replaceState wciągnąłby ją do
        // apvts.state, skąd przeciekłaby do zapisywanych presetów. Stan sprzed
        // MIDI Learn (bez MIDIMAP) zostawia bieżącą mapę w spokoju.
        if (auto* map = xml->getChildByName ("MIDIMAP"))
        {
            const juce::SpinLock::ScopedLockType lock (ccMapLock);
            std::fill (std::begin (ccMap), std::end (ccMap), nullptr);
            for (auto* m : map->getChildWithTagNameIterator ("MAP"))
                if (auto* p = apvts.getParameter (m->getStringAttribute ("param")))
                    ccMap[juce::jlimit (0, 127, m->getIntAttribute ("cc"))] = p;
            updateCcActive();

            xml->removeChildElement (map, true);
        }

        applyStateXml (*xml);
    }
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
        set (pre + "stretchmode", 0.0f);           // Golden
        set (pre + "goldint",  2.0f);              // Off
        set (pre + "tilt",     1.0f);              // neutralne widmo
    }

    // Złoty silnik wyłączony.
    set ("ringMix",  0.0f);
    set ("fmAmt",    0.0f);
    set ("subLevel", 0.0f);
    set ("unison",   0.0f);
    set ("pitchQuant", 0.0f);

    // Arp i delay wyłączone, wartości robocze do domyślnych.
    set ("arpOn", 0.0f);
    set ("arpDiv", 2.0f);                          // 1/16
    set ("arpLen", 4.0f);                          // 8 kroków
    set ("arpMode", 0.0f);                         // Up
    set ("arpWord", 0.0f);
    set ("arpVel", 0.0f);
    set ("dlyOn", 0.0f);
    set ("dlySync", 1.0f);
    set ("dlyDiv", 1.0f);                          // 1/8
    set ("dlyTime", 400.0f);
    set ("dlyFeedback", 0.45f);
    set ("dlyMix", 0.25f);

    // Efektor wyłączony, amounty do zera.
    set ("fxOn", 0.0f);
    set ("fxDist", 0.0f);
    set ("fxSat", 0.0f);
    set ("fxShape", 0.0f);
    set ("fxRevSize", 0.5f);
    set ("fxRevMix", 0.0f);

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
    set ("lfoDrift", 0.0f);

    set ("envSync", 0.0f);                         // bez sync do tempa

    // Gate i stereo wyłączone, czysty pattern.
    set ("gateOn", 0.0f);
    set ("gateDepth", 0.85f);
    set ("spread", 0.0f);
    gateFlips.store (0);

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
