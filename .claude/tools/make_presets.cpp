// Generator presetów fabrycznych: konfiguruje realny procesor i zapisuje
// przez savePreset() (ten sam serializer co GUI/DAW — pliki na pewno wczytywalne).
// Po każdym zapisie krótki render-smoke: preset musi wydawać dźwięk (drukuje RMS,
// więc od razu widać odstające głośności).
//
// Bank ~115 presetów w kategoriach (prefiks 2-literowy grupuje listę w GUI):
//   BS Bass · LD Leads · PD Pads · PL Plucks · KY Keys & Organs ·
//   BL Bells & Metals · ST Ensemble · DR Drones · SQ Sequences ·
//   FX Efekty · RV Reverb Spaces · DI Destroyed
//
// Reguła banku: każdy preset ma JEDNEGO głównego bohatera (stretch albo ring
// albo phyllotaxis albo FM albo word...), fundament tonalny zostaje słyszalny,
// szerokość wg roli (bas wąsko, pad szeroko). Najciekawsze patche to morfologia
// w czasie: Env1→Stretch (materiał), Env2→Tilt (jasność), Env3→FM (energia).
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PartialTables.h"
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static FiSynthAudioProcessor* gProc = nullptr;

static void P (const juce::String& id, float plain)
{
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (gProc->apvts.getParameter (id)))
        rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
    else
        std::printf ("!! BRAK PARAMETRU %s\n", id.toRawUTF8());
}

// Obwiednia idx: lista punktów {czas s, poziom, krzywizna} + indeks sustain.
static void Env (int idx, std::initializer_list<EnvPoint> pts, int sustainIdx)
{
    auto& m = gProc->getEnvelopeModel (idx);
    m.points.assign (pts);
    m.sustainIndex = sustainIdx;
    m.sortAndClamp();
    gProc->commitEnvelope (idx);
}

// === Skróty konfiguracyjne (wszystkie wartości plain jak w GUI) ===

// resetToInit + wspólne defaulty; każdy preset zaczyna z czystej karty.
static void init (float gain, float bpm = 120.0f)
{
    gProc->resetToInit();
    P ("gain", gain);
    P ("bpm", bpm);
    P ("tempoSync", 1);
}

// Oscylator o (1..3): waveform, mix, stretch+tryb, tilt, goldint (0..4, 2=Off),
// detune w CENTACH (±48).
static void O (int o, int wave, float mix, float stretch = 0.0f, int mode = 0,
               float tilt = 1.0f, int goldint = 2, float detCents = 0.0f)
{
    const juce::String p = "osc" + juce::String (o);
    P (p + "waveform", (float) wave);
    P (p + "mix", mix);
    P (p + "stretch", stretch);
    P (p + "stretchmode", (float) mode);
    P (p + "tilt", tilt);
    P (p + "goldint", (float) goldint);
    P (p + "detune", detCents);
}

static void Filt (int type, float cutoff, float res)
{
    P ("filterType", (float) type);
    P ("filterCutoff", cutoff);
    P ("filterResonance", res);
}

// Efektor: dist/sat/fold 0..1 (0 = stopień neutralny), reverb mix+size.
static void FX (float dist, float sat, float fold, float revMix = 0.0f, float revSize = 0.5f)
{
    P ("fxOn", 1);
    P ("fxDist", dist);
    P ("fxSat", sat);
    P ("fxShape", fold);
    P ("fxRevMix", revMix);
    P ("fxRevSize", revSize);
}

// Golden Delay: sync do podziału (0=1/4 1=1/8 2=1/16 3=1/32 4=1/8T 5=1/16T)...
static void Dly (int div, float fb, float mix)
{
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", (float) div);
    P ("dlyFeedback", fb); P ("dlyMix", mix);
}

// ...albo wolny czas w ms.
static void DlyMs (float ms, float fb, float mix)
{
    P ("dlyOn", 1); P ("dlySync", 0); P ("dlyTime", ms);
    P ("dlyFeedback", fb); P ("dlyMix", mix);
}

static void Lfo (float rateHz, float depth, int shape = 0, float drift = 0.0f)
{
    P ("lfoSync", 0); P ("lfoRate", rateHz); P ("lfoDepth", depth);
    P ("lfoShape", (float) shape); P ("lfoDrift", drift);
}

static void LfoSync (int divIdx, float depth, int shape = 0, float drift = 0.0f)
{
    P ("lfoSync", 1); P ("lfoRateDiv", (float) divIdx); P ("lfoDepth", depth);
    P ("lfoShape", (float) shape); P ("lfoDrift", drift);
}

// Slot modulacji (1..3): dest wg ModDest (1 cutoff, 2 pitch, 3 oscmix, 4 rezo,
// 5..7 stretch1..3, 8 FM, 9..11 tilt1..3), amount −1..1.
static void Mod (int slot, int dest, float amt)
{
    const juce::String p = "env" + juce::String (slot);
    P (p + "Dest", (float) dest);
    P (p + "Amt", amt);
}

// Arpeggiator: div (tabela podziałów), lenIdx 0..5 -> 1/2/3/5/8/13 kroków,
// mode 0 Up, 1 Down, 2 UD-In, 3 UD-Ex, 4 Random φ, 5 Random, 6 Fib Walk.
static void Arp (int div, int lenIdx, int mode, bool word = false, float vel = 0.0f)
{
    P ("arpOn", 1); P ("arpDiv", (float) div); P ("arpLen", (float) lenIdx);
    P ("arpMode", (float) mode); P ("arpWord", word ? 1.0f : 0.0f); P ("arpVel", vel);
}

// Gate Fibonacciego: gen 0..4 -> 5/8/13/21/34 kroków; flips = ręcznie
// odwrócone kroki patternu (maska bitowa, jak klik w kropkę pierścienia).
static void Gate (float depth, int div, int gen, juce::uint64 flips = 0)
{
    P ("gateOn", 1); P ("gateDepth", depth);
    P ("gateDiv", (float) div); P ("gateGen", (float) gen);
    gProc->gateFlips.store (flips);
}

// === Kształty obwiedni amplitudy ===

// Perkusyjny: klik → 1 → cisza (sustain na poziomie 0 — nuta wybrzmiewa sama).
static void AmpPerc (float att, float dec)
{
    Env (0, { {0, 0, 0}, {att, 1, 0}, {att + dec, 0, -0.45f} }, 2);
}

// Klasyczny ADSR (sustain > 0, release po puszczeniu).
static void AmpADSR (float att, float dec, float sus, float rel)
{
    Env (0, { {0, 0, 0}, {att, 1, 0}, {att + dec, sus, -0.35f},
              {att + dec + rel, 0, -0.3f} }, 2);
}

// Pad: powolny swell do pełna (sustain u szczytu), długi release.
static void AmpPad (float att, float rel)
{
    Env (0, { {0, 0, 0}, {att, 1, 0.2f}, {att + rel, 0, -0.3f} }, 1);
}

// Kaskada 1/φᵏ (jak przycisk φ-casc w GUI): samopodobny decay struny.
static void AmpCasc (float att = 0.005f, float firstSeg = 0.45f)
{
    auto& m = gProc->getEnvelopeModel (0);
    m.points.clear();
    m.points.push_back ({ 0.0f, 0.0f, 0.0f });
    m.points.push_back ({ att, 1.0f, 0.0f });
    float t = att, dur = firstSeg;
    const float phi = (float) fiPhi;
    for (int k = 1; k <= 5; ++k)
    {
        t += dur;
        m.points.push_back ({ t, std::pow (phi, (float) -k), -0.35f });
        dur /= phi;
    }
    m.points.push_back ({ t + dur, 0.0f, -0.35f });
    m.sustainIndex = (int) m.points.size() - 1;
    m.sortAndClamp();
    gProc->commitEnvelope (0);
}

// === Kształty obwiedni modulacyjnych ===

// 1 → 0: transjent (FM/tilt/stretch na ataku).
static void EnvFall (int idx, float t)
{
    Env (idx, { {0, 1, 0}, {t, 0, -0.4f} }, 1);
}

// 0 → 1 (sustain u góry): powolna przemiana materiału.
static void EnvRise (int idx, float t)
{
    Env (idx, { {0, 0, 0}, {t, 1, 0.25f} }, 1);
}

// 0 → 1 → 0: pęcznienie.
static void EnvSwell (int idx, float up, float down)
{
    Env (idx, { {0, 0, 0}, {up, 1, 0}, {up + down, 0, -0.3f} }, 2);
}

// Render-smoke: akord C3+G3 przez ~0.7 s; zwraca RMS (0 = preset niemy).
static double smoke()
{
    constexpr int blk = 512;
    juce::AudioBuffer<float> buf (2, blk);
    double acc = 0.0; size_t cnt = 0;
    for (int b = 0; b < 60; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
        }
        gProc->processBlock (buf, midi);
        for (int i = 0; i < blk; ++i)
        {
            const double s = 0.5 * (buf.getSample (0, i) + buf.getSample (1, i));
            acc += s * s; ++cnt;
        }
    }
    // zdejmij nuty i wycisz ogon (delay/reverb/release) przed kolejnym presetem
    for (int b = 0; b < 500; ++b)
    {
        juce::MidiBuffer m;
        if (b == 0)
            m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        gProc->processBlock (buf, m);
    }
    return std::sqrt (acc / (double) cnt);
}

static int failures = 0;
static int count = 0;
static void save (const juce::String& name)
{
    const bool saved = gProc->savePreset (name);
    const double rms = smoke();
    const bool ok = saved && rms > 1.0e-4;
    std::printf ("%s  rms=%.3f  %s\n", ok ? "PASS" : "FAIL", rms, name.toRawUTF8());
    if (! ok) ++failures;
    ++count;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initJuce;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    gProc = dynamic_cast<FiSynthAudioProcessor*> (proc.get());
    gProc->setPlayConfigDetails (0, 2, 48000.0, 512);
    gProc->prepareToPlay (48000.0, 512);

    // ================================================================
    // BS · BASS (12)
    // ================================================================

    // Firmowy dół: klasyczny saw+even, ale sub siedzi na f/φ — obcy ciężar.
    init (0.62f);
    O (1, 3, 0.70f, 0.03f, 0, 1.5f);
    O (2, 8, 0.35f, 0, 0, 1.3f, 2, -5);
    O (3, 0, 0.30f);
    P ("subLevel", 0.40f); P ("unison", 0.12f); P ("spread", 0.10f);
    Filt (0, 700, 0.8f);
    FX (0, 0.15f, 0, 0, 0.3f);
    AmpADSR (0.003f, 0.25f, 0.75f, 0.20f);
    save ("BS Phi Foundation");

    // Czysty sub z ledwo wyczuwalnym złotym transientem FM.
    init (0.68f);
    O (1, 0, 0.90f);
    P ("subLevel", 0.90f); P ("fmAmt", 0.03f); P ("spread", 0);
    Filt (0, 3000, 0.3f);
    Mod (2, 8, 0.35f); EnvFall (2, 0.08f);
    AmpADSR (0.010f, 0.10f, 0.95f, 0.15f);
    save ("BS Golden Sub Pressure");

    // Reese z chóru Golden Detune zamiast dwóch sawów — quasi-aperiodyczny.
    init (0.55f);
    O (1, 3, 0.60f, 0.30f, 3, 1.5f);
    O (2, 3, 0.55f, 0, 0, 1.5f, 2, 10);
    O (3, 1, 0.40f, 0, 0, 1.7f, 2, -10);
    P ("unison", 0.60f); P ("spread", 0.50f); P ("subLevel", 0.25f);
    Filt (0, 900, 1.2f);
    Lfo (0.4f, 0.15f, 2, 0.40f);
    FX (0, 0.20f, 0, 0, 0.3f);
    AmpADSR (0.005f, 0.3f, 0.85f, 0.25f);
    save ("BS Fib Reese");

    // Lucas Comb: pusty dół (1,3,4,7...), sine pilnuje fundamentu.
    init (0.62f);
    O (1, 11, 0.80f, 0.28f, 7, 1.7f);
    O (2, 0, 0.45f);
    P ("ringMix", 0.08f); P ("spread", 0.20f);
    Filt (0, 1200, 1.0f);
    Mod (1, 5, 0.40f); EnvFall (1, 0.25f);
    AmpADSR (0.004f, 0.35f, 0.70f, 0.20f);
    save ("BS Lucas Hollow");

    // Metaliczny atak (FM+rezonansowe kopnięcie), tonalny korpus.
    init (0.60f);
    O (1, 6, 0.75f, 0.20f, 5, 1.2f);
    O (2, 0, 0.50f);
    P ("fmAmt", 0.15f); P ("spread", 0.12f);
    Filt (0, 1000, 2.2f);
    Mod (1, 1, 0.35f); EnvFall (1, 0.12f);
    Mod (2, 8, 0.50f); EnvFall (2, 0.09f);
    AmpADSR (0.002f, 0.30f, 0.45f, 0.15f);
    save ("BS Bronze Bite");

    // Spektralna "mowa": BP + trzy obwiednie mieszające widmo trzech oscylatorów.
    init (0.58f);
    O (1, 3, 0.60f, 0, 0, 1.2f);
    O (2, 7, 0.50f, 0, 0, 1.0f);
    O (3, 10, 0.40f, 0, 1, 0.9f);
    Filt (1, 800, 2.5f);
    Lfo (3.5f, 0.35f, 2);
    Mod (1, 3, 0.50f); Env (1, { {0,0,0}, {0.15f,1,0}, {0.4f,0.3f,0}, {0.8f,0.8f,0} }, 3);
    Mod (2, 9, -0.45f); EnvSwell (2, 0.2f, 0.5f);
    Mod (3, 7, 0.35f); EnvRise (3, 0.6f);
    AmpADSR (0.004f, 0.2f, 0.8f, 0.2f);
    save ("BS Talking Partial");

    // Acid: rezonansowy sweep + gate 13 kroków + przester z efektora.
    init (0.55f, 130);
    O (1, 3, 0.85f, 0, 0, 1.3f);
    P ("spread", 0.05f);
    Filt (0, 300, 4.5f);
    Mod (1, 1, 0.60f); EnvFall (1, 0.18f);
    Gate (0.30f, 2, 2);
    DlyMs (120, 0.30f, 0.12f);
    FX (0.30f, 0.30f, 0, 0.05f, 0.3f);
    AmpADSR (0.002f, 0.25f, 0.30f, 0.10f);
    save ("BS Golden Acid");

    // Gruba napięta stalowa struna: Golden Stiff + kaskada φ.
    init (0.65f);
    O (1, 2, 0.85f, 0.30f, 6, 1.6f);
    O (2, 9, 0.20f, 0, 0, 1.0f);
    P ("spread", 0.20f); P ("subLevel", 0.20f);
    Filt (0, 2500, 0.7f);
    Mod (1, 5, 0.50f); EnvFall (1, 0.20f);
    AmpCasc (0.003f, 0.5f);
    save ("BS Stiff Wire");

    // Industrialny grinder: Bronze+GoldShift w ring, sub trzyma wagę,
    // efektor rozgrzany (sat→dist→fold).
    init (0.42f);
    O (1, 3, 0.60f, 0.15f, 5, 1.1f);
    O (2, 6, 0.50f, 0.08f, 8, 1.0f);
    O (3, 0, 0.55f);
    P ("ringMix", 0.15f); P ("fmAmt", 0.08f); P ("subLevel", 0.25f); P ("spread", 0.15f);
    Filt (1, 700, 1.5f);
    Mod (1, 1, 0.40f); EnvFall (1, 0.3f);
    Mod (2, 8, 0.45f); EnvFall (2, 0.08f);
    Mod (3, 9, -0.60f); EnvFall (3, 0.15f);
    FX (0.55f, 0.30f, 0.15f, 0.05f, 0.3f);
    AmpADSR (0.003f, 0.3f, 0.7f, 0.15f);
    save ("BS Phi Grinder");

    // Neuro: fold+dist na BP z LFO 1/8, widmo ruszane trzema obwiedniami.
    init (0.45f, 172);
    O (1, 3, 0.55f, 0.25f, 3, 1.2f);
    O (2, 8, 0.50f, 0, 0, 1.0f);
    O (3, 10, 0.40f, 0, 1, 0.9f);
    P ("unison", 0.40f); P ("spread", 0.40f); P ("ringMix", 0.25f); P ("fmAmt", 0.30f);
    Filt (1, 900, 2.8f);
    LfoSync (3, 0.45f, 2, 0.30f);
    Mod (1, 7, 0.40f); EnvSwell (1, 0.3f, 0.6f);
    Mod (2, 8, 0.50f); EnvFall (2, 0.15f);
    Mod (3, 4, 0.40f); EnvRise (3, 0.5f);
    FX (0.40f, 0.20f, 0.45f, 0, 0.3f);
    AmpADSR (0.003f, 0.2f, 0.85f, 0.12f);
    save ("BS Golden Neuro");

    // Sinus po wavefoldzie = bogaty bas bez jednego sawa; goldint dodaje iskrę.
    init (0.50f);
    O (1, 0, 0.90f);
    O (2, 0, 0.12f, 0, 0, 1.0f, 3);
    P ("subLevel", 0.70f); P ("fmAmt", 0.10f); P ("spread", 0);
    Filt (0, 2500, 0.5f);
    FX (0, 0.15f, 0.55f, 0, 0.3f);
    AmpADSR (0.004f, 0.15f, 0.9f, 0.15f);
    save ("BS Folded Sub");

    // Prosty ciepły analog — kontrast dla matematycznej egzotyki.
    init (0.62f);
    O (1, 1, 0.55f, 0, 0, 1.8f);
    O (2, 2, 0.50f, 0, 0, 1.6f);
    P ("subLevel", 0.20f); P ("spread", 0);
    Filt (0, 750, 0.8f);
    FX (0, 0.40f, 0.08f, 0, 0.3f);
    AmpADSR (0.004f, 0.2f, 0.8f, 0.18f);
    save ("BS Warm Circuit");

    // ================================================================
    // LD · LEADS (10)
    // ================================================================

    // Punkt wejścia: klasyczny lead, złoty tylko w detalach (unison, drift).
    init (0.60f);
    O (1, 3, 0.65f, 0, 0, 1.2f);
    O (2, 2, 0.45f, 0.12f, 3, 1.2f);
    O (3, 0, 0.35f);
    P ("unison", 0.30f); P ("spread", 0.25f);
    Filt (0, 4500, 1.2f);
    Lfo (5.0f, 0.12f, 0, 0.2f);
    Dly (1, 0.35f, 0.20f);
    AmpADSR (0.01f, 0.3f, 0.8f, 0.25f);
    save ("LD Golden Mono");

    // Schodkowa obwiednia Pitch z kwantem 833¢ — złote ozdobniki zamiast glide.
    init (0.58f);
    O (1, 3, 0.70f, 0, 0, 1.3f);
    O (2, 1, 0.35f, 0, 0, 1.2f, 3);
    O (3, 2, 0.30f, 0, 0, 1.2f, 1);
    P ("spread", 0.30f); P ("pitchQuant", 1);
    Filt (0, 5000, 1.0f);
    Mod (1, 2, 1.0f);
    Env (1, { {0,1,0}, {0.16f,1,0}, {0.20f,0.694f,0}, {0.36f,0.694f,0},
              {0.40f,0.347f,0}, {0.56f,0.347f,0}, {0.60f,0,0} }, 6);
    Dly (1, 0.45f, 0.30f);
    AmpADSR (0.005f, 0.2f, 0.85f, 0.2f);
    save ("LD 833 Hero");

    // Przenikliwy industrial: ring+FM transjent, wysoki rezonans BP.
    init (0.52f);
    O (1, 3, 0.60f, 0, 0, 0.8f);
    O (2, 6, 0.45f, 0, 0, 1.0f);
    P ("ringMix", 0.20f); P ("fmAmt", 0.12f); P ("spread", 0.40f);
    Filt (1, 2400, 3.5f);
    Mod (1, 8, 0.45f); EnvFall (1, 0.10f);
    Dly (2, 0.35f, 0.22f);
    FX (0.15f, 0.20f, 0, 0.08f, 0.4f);
    AmpADSR (0.004f, 0.25f, 0.75f, 0.15f);
    save ("LD Spiral Screamer");

    // Organiczny dęty: sine+cichy triangle, miękki atak, oddech LFO.
    init (0.65f);
    O (1, 0, 0.85f);
    O (2, 2, 0.25f, 0, 0, 1.4f);
    O (3, 9, 0.20f, 0, 0, 2.2f);
    P ("spread", 0.20f); P ("subLevel", 0.10f);
    Filt (0, 3200, 0.6f);
    Lfo (4.5f, 0.08f, 0, 0.3f);
    Dly (1, 0.25f, 0.10f);
    AmpADSR (0.09f, 0.3f, 0.85f, 0.25f);
    save ("LD Fib Flute");

    // Cienki szklany lead z czytelną nutą.
    init (0.64f);
    O (1, 0, 0.75f);
    O (2, 9, 0.45f, 0.20f, 0, 0.9f);
    P ("fmAmt", 0.08f); P ("spread", 0.70f);
    Filt (1, 2000, 1.2f);
    DlyMs (140, 0.30f, 0.18f);
    AmpADSR (0.006f, 0.4f, 0.6f, 0.2f);
    save ("LD Glass Needle");

    // Polysynth z dziurami Lucasa w widmie.
    init (0.60f);
    O (1, 3, 0.60f, 0, 0, 1.2f);
    O (2, 11, 0.50f, 0.15f, 7, 1.1f);
    O (3, 0, 0.35f);
    P ("unison", 0.25f); P ("spread", 0.35f);
    Filt (0, 2200, 1.4f);
    Mod (1, 1, 0.45f); EnvFall (1, 0.5f);
    AmpADSR (0.008f, 0.35f, 0.7f, 0.2f);
    save ("LD Lucas Prophet");

    // Zjeżdżający laser: pitch −24 st + FM decay; do gry i do efektów.
    init (0.55f);
    O (1, 2, 0.80f, 0, 0, 0.9f);
    P ("fmAmt", 0.35f); P ("spread", 0.25f);
    Filt (0, 8000, 1.0f);
    Mod (1, 2, -1.0f); EnvRise (1, 0.35f);
    Mod (2, 8, 0.50f); EnvFall (2, 0.12f);
    Dly (2, 0.45f, 0.30f);
    AmpPerc (0.002f, 0.5f);
    save ("LD Golden Laser");

    // Gitarowo-syntetyczny shredder na saturacji i dist.
    init (0.48f);
    O (1, 3, 0.60f, 0, 0, 1.1f);
    O (2, 6, 0.45f, 0, 0, 1.1f, 3);
    O (3, 2, 0.35f, 0.10f, 3, 1.2f);
    P ("unison", 0.40f); P ("spread", 0.35f); P ("fmAmt", 0.15f);
    Filt (0, 3600, 1.3f);
    Mod (1, 1, 0.30f); EnvFall (1, 0.4f);
    Dly (1, 0.30f, 0.18f);
    FX (0.40f, 0.15f, 0.08f, 0.15f, 0.5f);
    AmpADSR (0.004f, 0.3f, 0.85f, 0.10f);
    save ("LD Phi Shredder");

    // Szkło po foldingu: ostre lśniące harmoniczne w dużej jasnej przestrzeni.
    init (0.55f);
    O (1, 0, 0.80f);
    O (2, 9, 0.40f, 0.20f, 0, 0.8f);
    P ("fmAmt", 0.10f); P ("spread", 0.70f);
    Filt (0, 9000, 0.8f);
    Dly (1, 0.30f, 0.15f);
    FX (0, 0, 0.40f, 0.30f, 0.75f);
    AmpADSR (0.005f, 0.4f, 0.7f, 0.3f);
    save ("LD Folded Glass");

    // Między brassem a syreną: Bronze w środku, saturation na wierzchu.
    init (0.52f);
    O (1, 3, 0.65f, 0.08f, 5, 1.4f);
    O (2, 7, 0.45f, 0, 0, 1.3f);
    P ("unison", 0.20f); P ("spread", 0.25f);
    Filt (0, 1800, 1.6f);
    Mod (1, 1, 0.45f); EnvRise (1, 0.15f);
    Mod (2, 9, -0.35f); EnvSwell (2, 0.1f, 0.4f);
    FX (0.10f, 0.45f, 0.06f, 0.10f, 0.4f);
    AmpADSR (0.07f, 0.2f, 0.85f, 0.2f);
    save ("LD Radiation Horn");

    // ================================================================
    // PD · PADS (13)
    // ================================================================

    // Wizytówka phyllotaxis: harmoniczne oscylatory, pełna szerokość,
    // barwa powoli rozkwita (tilt + stretch pod obwiedniami).
    init (0.58f);
    O (1, 3, 0.55f, 0, 0, 1.8f);
    O (2, 7, 0.50f, 0, 0, 1.6f, 2, -6);
    O (3, 9, 0.40f, 0, 0, 1.4f);
    P ("unison", 0.60f); P ("spread", 0.90f); P ("subLevel", 0.15f);
    Filt (0, 2800, 0.7f);
    Lfo (0.25f, 0.20f, 0, 0.6f);
    Mod (1, 9, -0.40f); EnvRise (1, 3.5f);
    Mod (2, 7, 0.15f); EnvRise (2, 5.0f);
    Dly (0, 0.50f, 0.35f);
    AmpPad (1.5f, 4.0f);
    save ("PD Phyllotaxis Bloom");

    // Między akordem a dronem: goldint ±φ na cichych oscylatorach.
    init (0.58f);
    O (1, 3, 0.60f, 0, 0, 1.7f);
    O (2, 2, 0.35f, 0, 0, 1.7f, 3);
    O (3, 2, 0.35f, 0, 0, 1.7f, 1);
    P ("unison", 0.30f); P ("spread", 0.70f);
    Filt (0, 2200, 0.6f);
    Dly (0, 0.60f, 0.35f);
    AmpPad (2.2f, 5.0f);
    save ("PD Golden Horizon");

    // Chmura partiali, która stale reorganizuje barwę (S&H + dwie obwiednie).
    init (0.56f);
    O (1, 12, 0.55f, 0.18f, 1, 2.0f);
    O (2, 10, 0.45f, 0.12f, 1, 2.2f);
    O (3, 0, 0.40f);
    P ("spread", 1.0f); P ("unison", 0.35f);
    Filt (0, 3000, 0.8f);
    Lfo (0.15f, 0.30f, 3, 0.5f);
    Mod (1, 5, 0.30f); EnvRise (1, 6.0f);
    Mod (2, 10, -0.35f); EnvRise (2, 8.0f);
    Dly (0, 0.55f, 0.40f);
    AmpPad (1.8f, 5.0f);
    save ("PD Fib Cloud");

    // Organowo-metaliczna przestrzeń: drawbar + srebrny triangle.
    init (0.56f);
    O (1, 7, 0.60f, 0, 0, 1.4f);
    O (2, 2, 0.45f, 0.15f, 4, 1.5f);
    O (3, 8, 0.35f, 0, 0, 1.6f);
    P ("unison", 0.30f); P ("spread", 0.90f);
    Filt (0, 3400, 0.6f);
    Dly (0, 0.55f, 0.40f);
    FX (0, 0.10f, 0, 0.30f, 0.85f);
    AmpPad (0.5f, 6.0f);
    save ("PD Silver Cathedral");

    // Najbardziej konwencjonalny analog — drift φ zamiast jawnej pętli LFO.
    init (0.58f);
    O (1, 3, 0.55f, 0.10f, 3, 1.5f);
    O (2, 3, 0.50f, 0.10f, 3, 1.5f, 2, 7);
    O (3, 2, 0.40f, 0, 0, 1.5f, 2, -6);
    P ("unison", 0.50f); P ("spread", 0.60f);
    Filt (0, 2400, 0.8f);
    Lfo (0.35f, 0.25f, 0, 0.9f);
    AmpPad (0.9f, 3.5f);
    save ("PD Analog Drift");

    // Atak = niestabilna materia (stretch wysoko), korpus krystalizuje w akord.
    init (0.56f);
    O (1, 3, 0.05f, 0, 0, 1.5f);
    O (2, 9, 0.50f, 0.05f, 0, 1.3f);
    O (3, 0, 0.40f);
    P ("spread", 0.80f); P ("unison", 0.30f);
    Filt (0, 2600, 0.8f);
    Mod (1, 5, 0.50f); EnvFall (1, 2.0f);
    Mod (2, 6, 0.45f); EnvFall (2, 3.2f);
    Mod (3, 8, 0.30f); EnvFall (3, 0.8f);
    Dly (0, 0.45f, 0.30f);
    AmpPad (0.15f, 4.0f);
    P ("osc1mix", 0.55f);
    save ("PD Inharmonic Sunrise");

    // Odwrotność: start harmoniczny, dźwięk rozpada się na metaliczne partiale.
    init (0.56f);
    O (1, 3, 0.55f, 0, 0, 1.9f);
    O (2, 2, 0.45f, 0, 0, 1.8f);
    O (3, 0, 0.40f);
    P ("spread", 0.85f); P ("unison", 0.25f);
    Filt (0, 2400, 0.7f);
    Mod (1, 5, 0.55f); EnvRise (1, 7.0f);
    Mod (2, 9, -0.50f); EnvRise (2, 9.0f);
    Mod (3, 8, 0.25f); EnvRise (3, 10.0f);
    Dly (0, 0.50f, 0.35f);
    AmpPad (0.8f, 5.0f);
    save ("PD Harmonic Dissolution");

    // Ciemny chór Fib Word: dziury słowa + duży unison, wolne S&H.
    init (0.56f);
    O (1, 7, 0.55f, 0, 0, 2.0f);
    O (2, 12, 0.50f, 0, 0, 2.1f);
    O (3, 8, 0.40f, 0, 0, 1.9f);
    P ("unison", 0.70f); P ("spread", 0.85f);
    Filt (0, 1500, 1.3f);
    Lfo (0.12f, 0.25f, 3, 0.4f);
    Dly (0, 0.45f, 0.35f);
    AmpPad (1.2f, 4.5f);
    save ("PD Dark Fib Choir");

    // Powietrze bez powtórek: jasny tilt, HP, drift 100%, tekstura nie pad.
    init (0.62f);
    O (1, 0, 0.50f);
    O (2, 2, 0.45f, 0, 0, 0.6f);
    O (3, 8, 0.40f, 0, 0, 0.5f);
    P ("spread", 1.0f); P ("unison", 0.40f);
    Filt (2, 600, 0.8f);
    Lfo (0.10f, 0.35f, 3, 1.0f);
    Dly (0, 0.55f, 0.40f);
    AmpPad (3.0f, 6.0f);
    save ("PD Non-Repeating Air");

    // Ciepła taśma: saturation skleja golden detune w miękki analogowy pad.
    init (0.56f);
    O (1, 3, 0.55f, 0.12f, 3, 1.6f);
    O (2, 2, 0.50f, 0, 0, 1.6f, 2, 5);
    O (3, 7, 0.35f, 0, 0, 1.5f);
    P ("unison", 0.35f); P ("spread", 0.60f);
    Filt (0, 2000, 0.7f);
    Lfo (0.3f, 0.18f, 0, 0.6f);
    Dly (1, 0.30f, 0.12f);
    FX (0.03f, 0.35f, 0, 0.28f, 0.7f);
    AmpPad (0.8f, 3.5f);
    save ("PD Warm Phi Tape");

    // Ściana dźwięku: distortion PRZED wielkim pogłosem.
    init (0.46f);
    O (1, 3, 0.55f, 0.10f, 3, 1.4f);
    O (2, 2, 0.50f, 0, 0, 1.4f, 2, 8);
    O (3, 7, 0.40f, 0, 0, 1.5f, 2, -7);
    P ("unison", 0.60f); P ("spread", 1.0f);
    Filt (0, 2600, 0.8f);
    Lfo (0.2f, 0.2f, 0, 0.7f);
    Dly (0, 0.45f, 0.30f);
    FX (0.30f, 0.20f, 0.06f, 0.60f, 0.95f);
    AmpPad (1.0f, 5.0f);
    save ("PD Shoegaze Phi");

    // Chór po delikatnym foldzie — formantopodobne górne harmoniczne.
    init (0.54f);
    O (1, 7, 0.55f, 0, 0, 1.6f);
    O (2, 8, 0.50f, 0, 0, 1.6f);
    O (3, 0, 0.40f);
    P ("unison", 0.40f); P ("spread", 1.0f);
    Filt (0, 2000, 0.7f);
    Dly (0, 0.40f, 0.20f);
    FX (0, 0.20f, 0.15f, 0.50f, 0.9f);
    AmpPad (1.4f, 5.0f);
    save ("PD Folded Choir");

    // Chór mgławicy: golden detune przez wędrujący band-pass (port z 12).
    init (0.60f);
    O (1, 8, 0.55f, 0.50f, 3, 1.6f);
    O (2, 2, 0.45f, 0.50f, 3, 1.8f, 2, 5);
    O (3, 0, 0.28f, 0, 0, 1.0f, 4);
    P ("unison", 0.75f); P ("spread", 0.95f); P ("subLevel", 0.20f);
    Filt (1, 900, 1.3f);
    Mod (2, 1, 0.50f); Env (2, { {0,0,0}, {3.0f,1,0.3f}, {8.0f,0.25f,-0.2f} }, 1);
    Lfo (0.3f, 0.25f, 2, 0.65f);
    Dly (0, 0.55f, 0.40f);
    AmpPad (0.8f, 4.5f);
    save ("PD Nebula Chorale");

    // ================================================================
    // PL · PLUCKS (10)
    // ================================================================

    // Firmowa struna: Golden Pluck (szarpnięcie w 1/φ długości) + kaskada φ.
    init (0.65f);
    O (1, 9, 0.85f, 0, 0, 1.5f);
    O (2, 9, 0.30f, 0, 0, 1.3f, 3, 3);
    P ("spread", 0.40f); P ("subLevel", 0.15f);
    Filt (0, 6000, 0.7f);
    Mod (1, 9, 0.45f); EnvFall (1, 0.5f);
    Dly (1, 0.30f, 0.15f);
    AmpCasc();
    save ("PL Golden String");

    // Koto: sztywna struna + goldint iskra + krótki FM transjent.
    init (0.63f);
    O (1, 9, 0.80f, 0.15f, 6, 1.4f);
    O (2, 2, 0.25f, 0, 0, 1.2f, 3);
    P ("spread", 0.35f);
    Filt (0, 5000, 0.9f);
    Mod (2, 8, 0.40f); EnvFall (2, 0.06f);
    Dly (2, 0.35f, 0.22f);
    AmpPerc (0.002f, 0.7f);
    save ("PL Phi Koto");

    // Szklana kalimba: pluck + sine, złoty stretch, FM na ataku.
    init (0.65f);
    O (1, 9, 0.75f, 0.15f, 0, 1.0f);
    O (2, 0, 0.50f);
    P ("fmAmt", 0.10f); P ("spread", 0.70f);
    Filt (0, 8000, 0.6f);
    Mod (1, 8, 0.35f); EnvFall (1, 0.08f);
    Dly (1, 0.30f, 0.20f);
    AmpPerc (0.002f, 0.9f);
    save ("PL Glass Kalimba");

    // Marimba Fibonacciego: ciemny triangle + rzadki grzebień, tonalny "tuk".
    init (0.66f);
    O (1, 2, 0.80f, 0.08f, 1, 1.9f);
    O (2, 10, 0.45f, 0.08f, 1, 1.7f);
    P ("spread", 0.25f);
    Filt (0, 3000, 0.8f);
    Mod (1, 2, -0.04f); EnvFall (1, 0.05f);
    AmpPerc (0.002f, 0.35f);
    save ("PL Fib Marimba");

    // Cymbałki z brązu: metaliczne sidebandy ringu pod kaskadą φ.
    init (0.62f);
    O (1, 9, 0.70f, 0.15f, 5, 1.2f);
    O (2, 3, 0.35f, 0.15f, 5, 1.4f);
    P ("ringMix", 0.08f); P ("spread", 0.60f);
    Filt (0, 6500, 0.8f);
    Dly (1, 0.40f, 0.25f);
    AmpCasc (0.003f, 0.4f);
    save ("PL Bronze Dulcimer");

    // Samogrające krople: wysoki glass pluck + arp Random φ + długie echo.
    init (0.60f, 92);
    O (1, 0, 0.70f);
    O (2, 9, 0.50f, 0.30f, 0, 0.9f);
    P ("spread", 0.80f);
    Filt (0, 9000, 0.7f);
    Arp (1, 4, 4, true, 0.55f);
    Dly (0, 0.55f, 0.40f);
    AmpPerc (0.004f, 0.30f);
    save ("PL Rain Drop");

    // Suchy drewniany mallet — bez FM, bez delaya, wąsko.
    init (0.68f);
    O (1, 2, 0.85f, 0, 0, 1.6f);
    O (2, 10, 0.40f, 0, 1, 1.5f);
    P ("spread", 0.15f);
    Filt (0, 2600, 1.1f);
    AmpPerc (0.002f, 0.22f);
    save ("PL Wooden Fibonacci");

    // Fraktalna harfa: fold dodaje iskry, jasny reverb zamienia je w ogon.
    init (0.58f);
    O (1, 9, 0.75f, 0, 0, 0.8f);
    O (2, 10, 0.45f, 0, 1, 0.9f);
    P ("spread", 0.70f);
    Filt (0, 9000, 0.6f);
    Mod (1, 9, 0.40f); EnvFall (1, 0.4f);
    Dly (1, 0.35f, 0.20f);
    FX (0, 0.10f, 0.20f, 0.38f, 0.8f);
    AmpCasc (0.002f, 0.35f);
    save ("PL Fractal Harp");

    // Harfa złotych interwałów: trzy pluki w stosie −φ/0/+φ.
    init (0.62f);
    O (1, 9, 0.75f, 0, 0, 1.2f);
    O (2, 9, 0.45f, 0, 0, 1.1f, 3);
    O (3, 9, 0.35f, 0, 0, 1.3f, 1);
    P ("spread", 0.55f); P ("subLevel", 0.12f);
    Filt (0, 7000, 0.7f);
    Dly (1, 0.35f, 0.25f);
    AmpPerc (0.003f, 1.1f);
    save ("PL Phi Harp Cascade");

    // Neonowa kropla: Golden Octave (miękka kompresja oktaw) + echo triolowe.
    init (0.62f);
    O (1, 2, 0.75f, 0.35f, 2, 0.9f);
    O (2, 0, 0.45f);
    P ("spread", 0.50f); P ("fmAmt", 0.06f);
    Filt (0, 7500, 1.0f);
    Dly (4, 0.45f, 0.30f);
    AmpPerc (0.002f, 0.5f);
    save ("PL Neon Droplet");

    // ================================================================
    // KY · KEYS & ORGANS (10)
    // ================================================================

    // Nieistniejący fortepian: Golden Stiff + pluck w ataku, tilt gaśnie.
    init (0.62f);
    O (1, 3, 0.65f, 0.14f, 6, 1.6f);
    O (2, 0, 0.45f);
    O (3, 9, 0.35f, 0, 0, 0.9f);
    P ("spread", 0.30f); P ("subLevel", 0.10f);
    Filt (0, 6500, 0.6f);
    Mod (1, 11, -0.60f); EnvFall (1, 0.10f);
    Mod (2, 5, 0.12f); EnvFall (2, 0.8f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {0.35f,0.45f,-0.4f}, {1.4f,0.18f,-0.35f},
              {3.2f,0,-0.3f} }, 4);
    save ("KY Stiff Pianoid");

    // Rejestry drawbar w interwałach φ zamiast kwint — organ z innej liturgii.
    init (0.58f);
    O (1, 7, 0.70f, 0, 0, 1.4f);
    O (2, 7, 0.40f, 0, 0, 1.4f, 3);
    O (3, 7, 0.30f, 0, 0, 1.5f, 1);
    P ("unison", 0.15f); P ("spread", 0.60f);
    Filt (0, 5000, 0.5f);
    AmpADSR (0.005f, 0.05f, 0.95f, 0.12f);
    save ("KY Phi Drawbar Organ");

    // Organ z chorusowym ruchem bez LFO pitch: golden detune robi "leslie".
    init (0.58f);
    O (1, 7, 0.70f, 0.30f, 3, 1.3f);
    O (2, 7, 0.45f, 0.30f, 3, 1.4f, 2, 6);
    P ("unison", 0.30f); P ("spread", 0.80f);
    Filt (0, 5500, 0.5f);
    Dly (1, 0.25f, 0.12f);
    AmpADSR (0.006f, 0.05f, 0.95f, 0.12f);
    save ("KY Detuned Golden Organ");

    // Rejestry z trzech grzebieni: drawbar+Fib+Lucas, ciemny tilt.
    init (0.58f);
    O (1, 7, 0.65f, 0, 0, 1.7f);
    O (2, 10, 0.45f, 0, 1, 1.8f);
    O (3, 11, 0.35f, 0, 7, 1.9f);
    P ("spread", 0.70f);
    Filt (0, 3600, 0.6f);
    AmpADSR (0.008f, 0.05f, 0.95f, 0.15f);
    save ("KY Fib Register Organ");

    // EP na parzystych harmonicznych, dzwonkowy atak z FM.
    init (0.62f);
    O (1, 8, 0.65f, 0, 0, 1.5f);
    O (2, 0, 0.50f);
    O (3, 9, 0.30f, 0, 0, 0.9f);
    P ("fmAmt", 0.08f); P ("spread", 0.30f);
    Filt (0, 5000, 0.7f);
    Mod (1, 8, 0.35f); EnvFall (1, 0.07f);
    AmpADSR (0.003f, 1.2f, 0.35f, 0.35f);
    save ("KY Even Electric Piano");

    // Tine EP: sine+pluck, tłuste tiny z golden stretch 0.1.
    init (0.62f);
    O (1, 0, 0.70f);
    O (2, 9, 0.40f, 0.10f, 0, 1.1f);
    P ("fmAmt", 0.12f); P ("spread", 0.30f);
    Filt (0, 6000, 0.6f);
    Mod (2, 8, 0.35f); EnvFall (2, 0.12f);
    Dly (1, 0.25f, 0.12f);
    FX (0, 0.20f, 0, 0.10f, 0.4f);
    AmpADSR (0.002f, 1.4f, 0.30f, 0.3f);
    save ("KY Golden Tine EP");

    // Clav: nosowy pulse + słowo Fibonacciego, BP i szybki cutoff-spadek.
    init (0.66f, 110);
    O (1, 6, 0.70f, 0, 0, 0.9f);
    O (2, 12, 0.45f, 0, 0, 1.0f);
    P ("spread", 0.15f);
    Filt (1, 1200, 2.0f);
    Mod (1, 1, 0.45f); EnvFall (1, 0.15f);
    AmpADSR (0.002f, 0.5f, 0.45f, 0.08f);
    save ("KY Fib Clav");

    // Klawesyn obcych: pluck+pulse w klangorze Golden Shift, jasno i sucho.
    init (0.60f);
    O (1, 9, 0.70f, 0.15f, 8, 0.8f);
    O (2, 6, 0.45f, 0.15f, 8, 0.9f);
    P ("ringMix", 0.08f); P ("spread", 0.30f);
    Filt (0, 8000, 0.9f);
    DlyMs (90, 0.25f, 0.12f);
    AmpADSR (0.002f, 0.8f, 0.25f, 0.08f);
    save ("KY Alien Harpsichord");

    // Wurlitzer: even+sine+pluck przez asymetryczną saturację — "bark" w forte.
    init (0.60f);
    O (1, 8, 0.60f, 0, 0, 1.5f);
    O (2, 0, 0.50f);
    O (3, 9, 0.30f, 0, 0, 1.0f);
    P ("fmAmt", 0.08f); P ("spread", 0.25f);
    Filt (0, 4500, 0.7f);
    Mod (1, 8, 0.30f); EnvFall (1, 0.06f);
    FX (0.06f, 0.30f, 0.12f, 0.10f, 0.35f);
    AmpADSR (0.003f, 1.1f, 0.4f, 0.3f);
    save ("KY Golden Wurlitzer");

    // Trzy drawbary w saturacji — masa i kompresja sumy rejestrów.
    init (0.56f);
    O (1, 7, 0.70f, 0, 0, 1.4f);
    O (2, 7, 0.50f, 0.12f, 3, 1.4f, 2, 4);
    O (3, 7, 0.35f, 0, 0, 1.3f, 2, -5);
    P ("unison", 0.15f); P ("spread", 0.45f);
    Filt (0, 4200, 0.6f);
    FX (0, 0.35f, 0, 0.10f, 0.35f);
    AmpADSR (0.005f, 0.05f, 0.95f, 0.12f);
    save ("KY Saturated Drawbars");

    // ================================================================
    // BL · BELLS & METALS (12)
    // ================================================================

    // Czysty złoty dzwon: sine rozciągnięty do f·φⁿ.
    init (0.60f);
    O (1, 3, 0.70f, 0.80f, 0, 1.5f);
    O (2, 0, 0.45f);
    P ("fmAmt", 0.12f); P ("spread", 0.60f);
    Filt (0, 12000, 0.5f);
    Dly (1, 0.35f, 0.25f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {1.5f,0.3f,-0.5f}, {5.0f,0,-0.4f} }, 3);
    save ("BL Pure Golden Bell");

    // Srebrny dzwon świątynny — ostrzejszy od złotego (σ=1+√2).
    init (0.56f);
    O (1, 2, 0.65f, 0.65f, 4, 1.3f);
    O (2, 0, 0.40f);
    P ("ringMix", 0.18f); P ("fmAmt", 0.08f); P ("spread", 0.55f);
    Filt (2, 300, 0.7f);
    Mod (1, 8, 0.30f); EnvFall (1, 0.05f);
    Env (0, { {0,0,0}, {0.002f,1,0}, {2.0f,0.25f,-0.5f}, {6.0f,0,-0.4f} }, 3);
    save ("BL Silver Temple Bell");

    // Gong z brązu: wielki, ring 40%, widmo powoli się porządkuje.
    init (0.54f);
    O (1, 2, 0.60f, 0.80f, 5, 1.4f);
    O (2, 0, 0.45f, 0, 0, 1.0f, 1);
    P ("ringMix", 0.40f); P ("fmAmt", 0.15f); P ("spread", 1.0f);
    Filt (0, 9000, 0.6f);
    Mod (1, 8, 0.45f); EnvFall (1, 0.6f);
    Mod (2, 5, -0.35f); EnvRise (2, 5.0f);
    Dly (0, 0.30f, 0.15f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {3.0f,0.3f,-0.5f}, {9.0f,0,-0.4f} }, 3);
    save ("BL Bronze Gong");

    // Dzwonki Lucasa: mało fundamentu, dużo powietrza między partialami.
    init (0.60f);
    O (1, 11, 0.75f, 0.40f, 7, 1.1f);
    O (2, 0, 0.30f);
    P ("spread", 1.0f);
    Filt (0, 12000, 0.5f);
    Dly (1, 0.40f, 0.30f);
    AmpPerc (0.002f, 1.4f);
    save ("BL Lucas Chime");

    // Metalowa sztaba: Golden Shift = równe przesunięcia partiali, klangor.
    init (0.62f);
    O (1, 2, 0.75f, 0.30f, 8, 1.5f);
    O (2, 0, 0.35f);
    P ("ringMix", 0.10f); P ("spread", 0.45f);
    Filt (1, 1400, 1.0f);
    AmpPerc (0.002f, 0.9f);
    save ("BL Golden Shift Bar");

    // Pozytywka: grzebień Fibonacciego + arp 8 kroków w 1/16.
    init (0.62f, 100);
    O (1, 10, 0.80f, 0, 1, 0.8f, 4);
    O (2, 9, 0.30f, 0, 0, 1.0f, 3);
    P ("spread", 0.60f);
    Filt (0, 14000, 0.5f);
    Arp (2, 4, 0, false, 0.25f);
    Dly (1, 0.30f, 0.30f);
    AmpPerc (0.002f, 0.9f);
    save ("BL Fib Music Box");

    // Kryształowe wahadło: bardzo długi cichy ogon, S&H krąży po cutoffie.
    init (0.56f);
    O (1, 0, 0.60f);
    O (2, 9, 0.45f, 0.45f, 0, 1.0f);
    P ("spread", 0.70f);
    Filt (0, 6000, 1.0f);
    Lfo (0.4f, 0.30f, 3, 0.5f);
    Dly (0, 0.65f, 0.40f);
    Env (0, { {0,0,0}, {0.002f,1,0}, {6.0f,0,-0.5f} }, 2);
    save ("BL Crystal Pendulum");

    // Jeden dźwięk przechodzi przez trzy metale: Golden→Silver→Bronze.
    init (0.54f);
    O (1, 2, 0.60f, 0.55f, 0, 1.3f);
    O (2, 2, 0.45f, 0.45f, 4, 1.3f);
    O (3, 2, 0.40f, 0.50f, 5, 1.4f);
    P ("ringMix", 0.15f); P ("spread", 0.70f);
    Filt (0, 8000, 0.7f);
    Mod (1, 5, -0.55f); EnvRise (1, 3.0f);
    Mod (2, 6, -0.45f); EnvRise (2, 5.5f);
    Mod (3, 7, 0.40f); EnvSwell (3, 2.0f, 4.0f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {4.0f,0.4f,-0.4f}, {9.0f,0,-0.35f} }, 3);
    save ("BL Three-Metal Morph");

    // Wibrafon: even + stiff, tremolo z wolnego LFO na cutoffie.
    init (0.62f);
    O (1, 8, 0.70f, 0.10f, 6, 1.4f);
    O (2, 0, 0.45f);
    P ("spread", 0.40f);
    Filt (0, 4500, 0.8f);
    Lfo (4.2f, 0.25f, 0);
    Env (0, { {0,0,0}, {0.003f,1,0}, {2.5f,0.2f,-0.5f}, {5.0f,0,-0.4f} }, 3);
    save ("BL Golden Vibes");

    // Gong głębinowy: niski, ogromny, ciemny pogłos dopowiada resztę.
    init (0.52f);
    O (1, 2, 0.65f, 0.70f, 5, 2.0f);
    O (2, 0, 0.50f, 0, 0, 1.0f, 0);
    P ("ringMix", 0.25f); P ("subLevel", 0.40f); P ("spread", 0.90f);
    Filt (0, 2500, 0.8f);
    FX (0, 0.20f, 0, 0.40f, 1.0f);
    Env (0, { {0,0,0}, {0.005f,1,0}, {4.0f,0.25f,-0.5f}, {10.0f,0,-0.4f} }, 3);
    save ("BL Deep Space Gong");

    // Rura dzwonna: GoldShift przez band-pass — tubular bell z sąsiedniego wszechświata.
    init (0.58f);
    O (1, 3, 0.65f, 0.25f, 8, 1.2f);
    O (2, 0, 0.40f);
    P ("fmAmt", 0.10f); P ("spread", 0.55f);
    Filt (1, 1200, 1.2f);
    Mod (1, 8, 0.30f); EnvFall (1, 0.10f);
    Dly (1, 0.35f, 0.25f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {2.8f,0.25f,-0.5f}, {7.0f,0,-0.4f} }, 3);
    save ("BL Tubular Phi");

    // Misa: sine w łagodnym złotym rozciągnięciu, pęczniejący FM oddech.
    init (0.58f);
    O (1, 0, 0.75f);
    O (2, 2, 0.40f, 0.35f, 0, 1.6f);
    P ("fmAmt", 0.05f); P ("spread", 0.50f);
    Filt (0, 5000, 0.6f);
    Mod (1, 8, 0.20f); EnvSwell (1, 2.5f, 4.0f);
    Dly (0, 0.40f, 0.25f);
    Env (0, { {0,0,0}, {0.05f,1,0}, {7.0f,0,-0.45f} }, 2);
    save ("BL Prayer Bowl");

    // ================================================================
    // ST · ENSEMBLE (6)
    // ================================================================

    // Smyczki φ: golden detune + unison, sekcyjny attack.
    init (0.58f);
    O (1, 3, 0.60f, 0.15f, 3, 1.6f);
    O (2, 3, 0.50f, 0.15f, 3, 1.6f, 2, 6);
    O (3, 2, 0.40f, 0, 0, 1.5f, 2, -5);
    P ("unison", 0.50f); P ("spread", 1.0f);
    Filt (0, 2600, 0.8f);
    Mod (1, 1, 0.35f); EnvRise (1, 0.4f);
    Lfo (0.3f, 0.15f, 0, 0.5f);
    Dly (1, 0.25f, 0.15f);
    AmpADSR (0.45f, 0.3f, 0.85f, 0.6f);
    save ("ST Golden Strings");

    // Kameralnie: mniejszy spread, ciemny tilt, krótki release.
    init (0.58f);
    O (1, 3, 0.60f, 0.06f, 1, 1.9f);
    O (2, 12, 0.45f, 0, 0, 1.8f);
    O (3, 8, 0.40f, 0, 0, 1.7f);
    P ("unison", 0.30f); P ("spread", 0.45f);
    Filt (0, 2000, 0.7f);
    AmpADSR (0.25f, 0.3f, 0.8f, 0.3f);
    save ("ST Fib Chamber");

    // Demonstracja stereo partiali: dół centralny, góra szeroka, zero delaya.
    init (0.60f);
    O (1, 3, 0.55f, 0, 0, 1.4f, 2, 4);
    O (2, 2, 0.50f, 0, 0, 1.4f, 2, -6);
    O (3, 7, 0.40f, 0, 0, 1.5f, 2, 8);
    P ("unison", 0.25f); P ("spread", 1.0f);
    Filt (0, 3200, 0.7f);
    AmpADSR (0.30f, 0.2f, 0.85f, 0.4f);
    save ("ST Phyllo Ensemble");

    // Blacha: ciemny saw+square, cutoff otwiera się jak zadęcie.
    init (0.56f);
    O (1, 3, 0.65f, 0, 0, 1.8f);
    O (2, 1, 0.45f, 0, 0, 1.7f);
    P ("unison", 0.20f); P ("spread", 0.30f);
    Filt (0, 1200, 1.1f);
    Mod (1, 1, 0.55f); EnvRise (1, 0.12f);
    Mod (2, 9, -0.35f); EnvSwell (2, 0.15f, 0.5f);
    AmpADSR (0.08f, 0.25f, 0.85f, 0.25f);
    save ("ST Golden Brass");

    // Róg z metalicznym środkiem (Bronze 8%, śladowy ring).
    init (0.56f);
    O (1, 3, 0.60f, 0.08f, 5, 1.6f);
    O (2, 2, 0.50f, 0, 0, 1.5f, 3);
    P ("ringMix", 0.06f); P ("spread", 0.25f);
    Filt (0, 1500, 1.3f);
    Mod (1, 1, 0.40f); EnvRise (1, 0.15f);
    AmpADSR (0.10f, 0.3f, 0.8f, 0.3f);
    save ("ST Bronze Horn");

    // Aksamit: sekcja w saturacji, bez blasku, do ballad.
    init (0.56f);
    O (1, 3, 0.55f, 0.10f, 3, 2.1f);
    O (2, 2, 0.50f, 0, 0, 2.0f, 2, 5);
    O (3, 8, 0.35f, 0, 0, 1.9f);
    P ("unison", 0.40f); P ("spread", 0.70f);
    Filt (0, 1600, 0.7f);
    FX (0, 0.30f, 0, 0.20f, 0.6f);
    AmpADSR (0.35f, 0.3f, 0.85f, 0.5f);
    save ("ST Velvet Section");

    // ================================================================
    // DR · DRONES & ATMOSPHERES (10)
    // ================================================================

    // Nieskończona złota maszyna: trzy metale, stretch płynie kilkanaście sekund.
    init (0.54f);
    O (1, 3, 0.55f, 0.30f, 0, 2.4f);
    O (2, 2, 0.45f, 0.35f, 4, 2.5f);
    O (3, 2, 0.40f, 0.30f, 7, 2.3f);
    P ("spread", 1.0f); P ("unison", 0.40f); P ("subLevel", 0.25f);
    Filt (0, 1800, 0.8f);
    Lfo (0.10f, 0.30f, 3, 0.8f);
    Mod (1, 5, 0.40f); Env (1, { {0,0,0}, {12.0f,1,0.2f} }, 1);
    Mod (2, 6, -0.30f); Env (2, { {0,1,0}, {14.0f,0,-0.2f} }, 1);
    Mod (3, 7, 0.35f); Env (3, { {0,0,0}, {8.0f,1,0}, {15.0f,0,0} }, 1);
    Dly (0, 0.55f, 0.45f);
    AmpPad (2.0f, 6.0f);
    save ("DR Infinite Golden Machine");

    // Sama przestrzeń phyllotaxis: harmoniczne fale, fundament wycięty HP.
    init (0.60f);
    O (1, 3, 0.50f, 0, 0, 1.2f, 2, 5);
    O (2, 2, 0.50f, 0, 0, 1.1f, 2, -7);
    O (3, 8, 0.45f, 0, 0, 1.2f);
    P ("spread", 1.0f); P ("unison", 0.70f);
    Filt (2, 600, 0.7f);
    Lfo (0.12f, 0.25f, 0, 0.9f);
    Dly (0, 0.60f, 0.40f);
    AmpPad (3.0f, 7.0f);
    save ("DR Phyllotaxis Space");

    // Reaktor: grzebienie+słowo w band-passie, płytki gate tyka w tle.
    init (0.54f, 60);
    O (1, 12, 0.55f, 0, 0, 1.5f);
    O (2, 10, 0.45f, 0, 1, 1.6f);
    O (3, 11, 0.40f, 0, 7, 1.7f);
    P ("ringMix", 0.20f); P ("fmAmt", 0.15f); P ("spread", 0.80f);
    Filt (1, 700, 1.6f);
    Gate (0.20f, 1, 2);
    Lfo (0.15f, 0.20f, 3, 0.6f);
    Dly (0, 0.55f, 0.40f);
    AmpPad (2.5f, 6.0f);
    save ("DR Fib Reactor");

    // Ciemna materia: sub f/φ niesie, reszta ledwo świeci.
    init (0.56f);
    O (1, 0, 0.65f);
    O (2, 8, 0.45f, 0.08f, 0, 2.6f);
    P ("subLevel", 0.80f); P ("spread", 0.40f);
    Filt (0, 500, 0.8f);
    Lfo (0.08f, 0.12f, 3, 0.3f);
    Dly (0, 0.65f, 0.35f);
    AmpPad (2.0f, 6.0f);
    save ("DR Dark Matter");

    // Las metalowych obiektów gra sam: arp Random φ + word + płytki gate.
    init (0.54f, 70);
    O (1, 2, 0.60f, 0.50f, 4, 1.4f);
    O (2, 0, 0.40f);
    O (3, 9, 0.35f, 0.40f, 0, 1.2f);
    P ("fmAmt", 0.08f); P ("spread", 0.90f);
    Filt (0, 5000, 0.8f);
    Arp (0, 5, 4, true, 0.60f);
    Gate (0.25f, 1, 4);
    Dly (0, 0.60f, 0.45f);
    FX (0, 0.10f, 0, 0.35f, 0.85f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {4.0f,0,-0.5f} }, 2);
    save ("DR Metal Forest");

    // Erozja harmonii: czysty start, chaos narasta trzema obwiedniami.
    init (0.54f);
    O (1, 7, 0.60f, 0, 0, 1.6f);
    O (2, 3, 0.50f, 0, 0, 1.5f);
    P ("spread", 0.70f);
    Filt (0, 2400, 0.8f);
    Mod (1, 5, 0.60f); Env (1, { {0,0,0}, {10.0f,1,0.3f} }, 1);
    Mod (2, 9, -0.45f); Env (2, { {0,0,0}, {12.0f,1,0.3f} }, 1);
    Mod (3, 8, 0.40f); Env (3, { {0,0,0}, {14.0f,1,0.3f} }, 1);
    Dly (0, 0.50f, 0.35f);
    AmpPad (1.5f, 5.0f);
    save ("DR Harmonic Erosion");

    // Powrót do porządku: metaliczny pył zbiera się w harmoniczny ton.
    init (0.54f);
    O (1, 3, 0.55f, 0.70f, 0, 1.3f);
    O (2, 2, 0.45f, 0.60f, 4, 1.4f);
    O (3, 0, 0.40f);
    P ("spread", 0.85f);
    Filt (0, 3000, 0.8f);
    Mod (1, 5, -0.70f); Env (1, { {0,0,0}, {8.0f,1,0.3f} }, 1);
    Mod (2, 6, -0.60f); Env (2, { {0,0,0}, {10.0f,1,0.3f} }, 1);
    Mod (3, 9, 0.30f); Env (3, { {0,0,0}, {9.0f,1,0.3f} }, 1);
    Dly (0, 0.45f, 0.30f);
    AmpPad (1.2f, 5.0f);
    save ("DR Return to Order");

    // Czarne słońce: ogromna ciężka masa, saturation robi środek z dołu.
    init (0.50f);
    O (1, 0, 0.60f);
    O (2, 8, 0.50f, 0, 0, 2.8f);
    O (3, 2, 0.40f, 0.20f, 5, 2.5f);
    P ("subLevel", 0.70f); P ("spread", 0.35f);
    Filt (0, 700, 0.9f);
    Mod (1, 7, 0.25f); Env (1, { {0,0,0}, {11.0f,1,0.2f} }, 1);
    FX (0.20f, 0.55f, 0.10f, 0.45f, 0.9f);
    AmpPad (2.5f, 7.0f);
    save ("DR Black Sun");

    // Głęboki kosmiczny dron (port z 11 Deep Field Drone).
    init (0.60f);
    O (1, 8, 0.55f, 0.30f, 2, 2.2f);
    O (2, 10, 0.40f, 0.20f, 7, 2.4f, 1);
    O (3, 0, 0.35f, 0, 0, 1.0f, 3);
    P ("subLevel", 0.45f); P ("unison", 0.85f); P ("spread", 1.0f);
    Filt (0, 1500, 0.6f);
    Lfo (0.2f, 0.30f, 0, 0.9f);
    Mod (2, 5, 0.35f); Env (2, { {0,0,0}, {4.0f,1,0.2f}, {8.0f,0,-0.2f} }, 1);
    Mod (3, 1, 0.30f); Env (3, { {0,0,0}, {2.5f,1,0.3f}, {7.0f,0,0} }, 1);
    Dly (0, 0.60f, 0.35f);
    Env (0, { {0,0,0}, {1.2f,1,0.25f}, {6.0f,0,-0.3f} }, 1);
    save ("DR Deep Field");

    // Zdeformowany kosmos: word+Lucas+złoto pod lekkim przesterem w pogłosie.
    init (0.50f);
    O (1, 12, 0.55f, 0.25f, 0, 1.8f);
    O (2, 11, 0.45f, 0.20f, 7, 1.9f);
    O (3, 0, 0.40f);
    P ("ringMix", 0.15f); P ("fmAmt", 0.10f); P ("spread", 0.90f);
    Filt (0, 2000, 0.9f);
    Lfo (0.10f, 0.30f, 3, 0.7f);
    Mod (1, 5, 0.30f); EnvRise (1, 9.0f);
    Dly (0, 0.55f, 0.30f);
    FX (0.15f, 0.20f, 0.20f, 0.55f, 0.95f);
    AmpPad (2.0f, 6.0f);
    save ("DR Distorted Cosmos");

    // ================================================================
    // SQ · SEQUENCES & GENERATIVE (12)
    // ================================================================

    // Wizytówka generatywna: Fib Walk po 13 krokach, rytm ze słowa, gate 13.
    init (0.62f, 118);
    O (1, 9, 0.80f, 0, 0, 1.2f);
    O (2, 0, 0.40f);
    P ("spread", 0.45f);
    Filt (0, 6000, 0.8f);
    Arp (2, 5, 6, true, 0.50f);
    Gate (0.35f, 2, 2);
    Dly (1, 0.40f, 0.28f);
    AmpPerc (0.002f, 0.30f);
    save ("SQ Fib Walk Plucks");

    // Pozytywka Weyla: melodia nigdy się nie zapętla, akcenty też nie.
    init (0.62f, 96);
    O (1, 10, 0.75f, 0, 1, 0.8f, 4);
    O (2, 9, 0.35f, 0, 0, 1.0f, 3);
    P ("spread", 0.60f);
    Filt (0, 12000, 0.6f);
    Arp (2, 4, 4, true, 0.70f);
    Dly (1, 0.35f, 0.32f);
    AmpPerc (0.002f, 0.6f);
    save ("SQ Weyl Music Box");

    // Fibonacci w melodii, φ w harmonii: stos −φ/0/+φ + gate 21.
    init (0.60f, 112);
    O (1, 2, 0.65f, 0, 0, 1.2f);
    O (2, 2, 0.40f, 0, 0, 1.2f, 3);
    O (3, 2, 0.35f, 0, 0, 1.3f, 1);
    P ("spread", 0.50f);
    Filt (0, 4500, 1.0f);
    Arp (2, 4, 6, false, 0.35f);
    Gate (0.45f, 2, 3);
    Dly (1, 0.40f, 0.25f);
    AmpPerc (0.003f, 0.35f);
    save ("SQ Golden Intervals");

    // Puls, który się nie powtarza: gate 34 z odwróconymi krokami + Random φ.
    init (0.60f, 124);
    O (1, 6, 0.70f, 0, 0, 1.0f);
    O (2, 0, 0.45f);
    P ("spread", 0.40f);
    Filt (0, 3500, 1.3f);
    Arp (2, 4, 4, true, 0.45f);
    Gate (0.80f, 2, 4, (1ULL << 5) | (1ULL << 13) | (1ULL << 21) | (1ULL << 28));
    Dly (2, 0.35f, 0.20f);
    AmpPerc (0.002f, 0.25f);
    save ("SQ Non-Repeating Pulse");

    // Komputer basowy: suchy Fib Walk 5 kroków, cutoff chodzi po S&H.
    init (0.60f, 120);
    O (1, 3, 0.70f, 0, 0, 1.5f);
    O (2, 0, 0.45f);
    P ("subLevel", 0.30f); P ("spread", 0.10f);
    Filt (0, 900, 1.8f);
    LfoSync (3, 0.35f, 3);
    Arp (2, 3, 6, false, 0.25f);
    Gate (0.50f, 2, 2);
    AmpPerc (0.002f, 0.20f);
    save ("SQ Fib Bass Computer");

    // Spiralny deszcz: wysoki szklany pluck w wolnym 1/8, długie echo.
    init (0.58f, 84);
    O (1, 0, 0.65f);
    O (2, 9, 0.50f, 0.30f, 0, 0.8f);
    P ("spread", 1.0f);
    Filt (0, 10000, 0.7f);
    Arp (1, 4, 4, true, 0.60f);
    Dly (0, 0.60f, 0.45f);
    AmpPerc (0.003f, 0.4f);
    save ("SQ Spiral Rain");

    // Trance-gate: szeroki pad cięty patternem 21 z własnymi flipami.
    init (0.58f, 126);
    O (1, 3, 0.60f, 0.12f, 3, 1.4f);
    O (2, 3, 0.55f, 0.12f, 3, 1.4f, 2, -7);
    O (3, 3, 0.40f, 0, 0, 1.4f, 2, 7);
    P ("unison", 0.80f); P ("spread", 0.95f); P ("subLevel", 0.30f);
    Filt (0, 1800, 1.6f);
    Mod (2, 1, 0.55f); Env (2, { {0,0,0}, {2.5f,1,0.3f}, {4.5f,0,0} }, 1);
    Gate (0.85f, 2, 3, (1ULL << 4) | (1ULL << 11) | (1ULL << 17));
    Dly (1, 0.45f, 0.30f);
    AmpADSR (0.05f, 0.3f, 0.9f, 0.3f);
    save ("SQ Golden Trance Gate");

    // Dwa słowa Fibonacciego mijają się długo: arp 13 (word) × gate 21.
    init (0.60f, 116);
    O (1, 9, 0.75f, 0, 0, 1.1f);
    O (2, 2, 0.40f, 0.15f, 1, 1.2f);
    P ("spread", 0.55f);
    Filt (0, 5500, 0.9f);
    Arp (2, 5, 0, true, 0.50f);
    Gate (0.60f, 2, 3);
    Dly (1, 0.45f, 0.30f);
    AmpPerc (0.002f, 0.30f);
    save ("SQ Double Fibonacci");

    // Komputer z folderem: sinus + fold 50% gra jak cyfrowy chip.
    init (0.55f, 128);
    O (1, 0, 0.80f);
    O (2, 0, 0.30f, 0, 0, 1.0f, 3);
    P ("spread", 0.30f);
    Filt (0, 4000, 0.9f);
    Arp (2, 4, 4, false, 0.45f);
    Gate (0.70f, 2, 4);
    Dly (2, 0.35f, 0.25f);
    FX (0, 0, 0.50f, 0.10f, 0.4f);
    AmpPerc (0.002f, 0.22f);
    save ("SQ Wavefold Computer");

    // Acid + słowo: rezonansowy skwierczący sequence z przesterem.
    init (0.52f, 132);
    O (1, 3, 0.80f, 0, 0, 1.2f);
    P ("spread", 0.10f);
    Filt (0, 400, 4.0f);
    Mod (1, 1, 0.60f); EnvFall (1, 0.15f);
    Arp (2, 4, 6, true, 0.40f);
    Gate (0.50f, 2, 2);
    DlyMs (110, 0.30f, 0.15f);
    FX (0.40f, 0.25f, 0, 0.05f, 0.3f);
    AmpPerc (0.002f, 0.20f);
    save ("SQ Acid Word");

    // Radiolatarnia (port z 13 Pulsar Beacon): Random φ + echa triolowe.
    init (0.62f, 96);
    O (1, 9, 0.75f, 0, 0, 1.1f);
    O (2, 10, 0.35f, 0, 1, 1.3f, 3);
    O (3, 0, 0.25f, 0, 0, 1.0f, 1);
    P ("subLevel", 0.30f); P ("spread", 0.80f); P ("unison", 0.30f);
    Filt (0, 5500, 1.0f);
    Mod (2, 1, 0.40f); EnvFall (2, 0.5f);
    Arp (1, 2, 4, true, 0.70f);
    Dly (4, 0.65f, 0.45f);
    AmpPerc (0.003f, 1.0f);
    save ("SQ Pulsar Beacon");

    // Rój (port z 15 Xeno Hive): Silver/Bronze + stutter 34 kroków w 1/32.
    init (0.55f, 128);
    O (1, 3, 0.60f, 0.75f, 4, 0.8f);
    O (2, 3, 0.55f, 0.85f, 5, 0.75f, 2, -5);
    O (3, 5, 0.15f);
    P ("unison", 1.0f); P ("spread", 1.0f); P ("subLevel", 0.30f); P ("fmAmt", 0.30f);
    Filt (0, 1600, 2.5f);
    Mod (2, 1, 0.60f); Env (2, { {0,1,0}, {0.35f,0.15f,-0.4f}, {1.5f,0,0} }, 1);
    Mod (3, 6, -0.50f); Env (3, { {0,0,0}, {1.4f,1,0.2f} }, 1);
    Gate (0.90f, 3, 4);
    LfoSync (6, 0.35f, 1);
    Dly (2, 0.50f, 0.30f);
    AmpADSR (0.01f, 0.3f, 0.85f, 0.2f);
    save ("SQ Xeno Hive");

    // ================================================================
    // FX · EFEKTY (8)
    // ================================================================

    // Spadający mechanizm: pitch schodzi schodkami 833¢, FM gaśnie, stretch rośnie.
    init (0.55f);
    O (1, 2, 0.70f, 0.10f, 0, 1.2f);
    O (2, 0, 0.45f);
    P ("fmAmt", 0.30f); P ("spread", 0.50f); P ("pitchQuant", 1);
    Filt (0, 6000, 0.9f);
    Mod (1, 2, -1.0f);
    Env (1, { {0,0,0}, {0.3f,0,0}, {0.35f,0.174f,0}, {0.7f,0.174f,0},
              {0.75f,0.347f,0}, {1.1f,0.347f,0}, {1.15f,0.521f,0}, {1.6f,0.694f,0} }, 7);
    Mod (2, 8, -0.28f); EnvRise (2, 1.5f);
    Mod (3, 5, 0.55f); EnvRise (3, 2.0f);
    Dly (0, 0.60f, 0.45f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {3.5f,0,-0.4f} }, 2);
    save ("FX Golden Fall");

    // Uderzenie w metal: ring wysoko, niski pitch, stereo na full.
    init (0.52f);
    O (1, 2, 0.55f, 0.65f, 5, 1.2f);
    O (2, 2, 0.50f, 0.55f, 4, 1.3f, 1);
    P ("ringMix", 0.70f); P ("fmAmt", 0.20f); P ("subLevel", 0.40f); P ("spread", 1.0f);
    Filt (0, 4000, 1.0f);
    Mod (1, 8, 0.50f); EnvFall (1, 0.15f);
    FX (0, 0.20f, 0, 0.30f, 0.9f);
    Env (0, { {0,0,0}, {0.002f,1,0}, {5.0f,0,-0.5f} }, 2);
    save ("FX Metal Impact");

    // Teleport: szybkie schodki pitch w górę przez HP i szybki gate.
    init (0.62f, 140);
    O (1, 2, 0.75f, 0, 0, 0.8f);
    P ("spread", 0.60f); P ("pitchQuant", 1);
    Filt (2, 1000, 1.3f);
    Mod (1, 2, 1.0f);
    Env (1, { {0,0,0}, {0.10f,0,0}, {0.12f,0.347f,0}, {0.22f,0.347f,0},
              {0.24f,0.694f,0}, {0.34f,0.694f,0}, {0.36f,1,0} }, 6);
    Gate (0.85f, 3, 1);
    Dly (2, 0.55f, 0.40f);
    AmpPerc (0.002f, 0.8f);
    save ("FX Phi Teleport");

    // Maszyna budzi się: FM, stretch i jasność narastają, S&H po cutoffie.
    init (0.52f);
    O (1, 3, 0.60f, 0, 0, 2.0f);
    O (2, 8, 0.45f, 0, 0, 1.8f);
    P ("spread", 0.70f);
    Filt (0, 1200, 1.2f);
    Lfo (0.8f, 0.40f, 3, 0.5f);
    Mod (1, 8, 0.55f); EnvRise (1, 5.0f);
    Mod (2, 5, 0.50f); EnvRise (2, 7.0f);
    Mod (3, 9, -0.50f); EnvRise (3, 9.0f);
    DlyMs (450, 0.60f, 0.40f);
    AmpPad (4.0f, 5.0f);
    save ("FX Machine Awakening");

    // Zapaść wielokąta: sinus rozrywa się w cyfrową konstrukcję.
    init (0.48f);
    O (1, 0, 0.85f);
    P ("spread", 0.50f); P ("pitchQuant", 1);
    Filt (2, 800, 1.2f);
    Mod (1, 8, 0.70f); EnvRise (1, 3.0f);
    Mod (2, 5, 0.80f); EnvRise (2, 4.0f);
    Mod (3, 2, -0.35f);
    Env (3, { {0,0,0}, {1.0f,0,0}, {1.1f,0.25f,0}, {2.0f,0.25f,0},
              {2.1f,0.5f,0}, {3.0f,0.5f,0}, {3.1f,1,0} }, 6);
    FX (0.40f, 0, 0.50f, 0.55f, 1.0f);
    DlyMs (600, 0.70f, 0.40f);
    AmpPad (0.5f, 4.0f);
    save ("FX Polygon Collapse");

    // Gadający obcy (port z 14 Alien Larynx): formanty z BP + glossolalia 833¢.
    init (0.55f);
    O (1, 12, 0.65f, 0.40f, 8, 0.9f);
    O (2, 4, 0.50f, 0.30f, 4, 0.8f, 1);
    P ("subLevel", 0.35f); P ("ringMix", 0.60f); P ("fmAmt", 0.25f);
    P ("pitchQuant", 1);
    Filt (1, 750, 5.0f);
    Mod (1, 1, 0.55f);
    Env (1, { {0,0.2f,0}, {0.15f,1,-0.3f}, {0.35f,0.35f,0.3f}, {0.6f,0.8f,0},
              {0.9f,0.15f,0}, {1.3f,0.55f,0} }, 5);
    Mod (2, 8, 0.50f);
    Env (2, { {0,0,0}, {0.1f,1,0}, {0.25f,0.1f,0}, {0.5f,0.7f,0}, {0.85f,0,0} }, 4);
    Mod (3, 2, 0.70f);
    Env (3, { {0,0.6f,0}, {0.3f,0.6f,0}, {0.35f,1,0}, {0.6f,1,0},
              {0.65f,0.3f,0}, {0.95f,0.3f,0}, {1.0f,0,0} }, 6);
    LfoSync (3, 0.5f, 3);
    DlyMs (120, 0.5f, 0.3f);
    Env (0, { {0,0,0}, {0.02f,1,0}, {0.9f,0.7f,-0.2f}, {2.4f,0,-0.3f} }, 2);
    save ("FX Alien Larynx");

    // Syrena z tunelu (port z 16 Wormhole Screamer): ring+FM, pitch spada 833¢.
    init (0.50f);
    O (1, 1, 0.55f, 0.90f, 8, 0.7f);
    O (2, 12, 0.50f, 0.60f, 0, 0.9f, 4);
    O (3, 11, 0.30f, 0, 7, 1.0f, 0);
    P ("ringMix", 0.85f); P ("fmAmt", 0.45f); P ("subLevel", 0.55f);
    P ("unison", 0.80f); P ("spread", 1.0f); P ("pitchQuant", 1);
    Filt (1, 1200, 4.0f);
    Mod (1, 4, 0.80f); Env (1, { {0,0,0}, {1.5f,1,0.3f} }, 1);
    Mod (2, 8, 0.55f);
    Env (2, { {0,1,0}, {0.6f,0.2f,-0.4f}, {1.4f,0.9f,0}, {2.4f,0,0} }, 3);
    Mod (3, 2, 0.90f); Env (3, { {0,1,0}, {1.8f,0,-0.5f} }, 1);
    LfoSync (4, 0.6f, 3, 1.0f);
    DlyMs (75, 0.75f, 0.35f);
    Env (0, { {0,0,0}, {0.005f,1,0}, {2.5f,0.8f,-0.2f}, {4.5f,0,-0.3f} }, 2);
    save ("FX Wormhole Screamer");

    // Wszystko naraz, złote schodki — legendarny chaos (port z 10).
    init (0.50f);
    O (1, 3, 0.55f, 0.85f, 5, 0.6f);
    O (2, 12, 0.50f, 0.70f, 7, 1.0f, 4);
    O (3, 5, 0.12f);
    P ("ringMix", 0.7f); P ("fmAmt", 0.8f); P ("subLevel", 0.5f); P ("unison", 1.0f);
    P ("spread", 1.0f); P ("pitchQuant", 1);
    Mod (2, 8, 0.60f); Env (2, { {0,1,0}, {1.2f,0,-0.4f} }, 1);
    Mod (3, 2, 0.55f); Env (3, { {0,0,0}, {0.8f,1,0}, {1.6f,0,0} }, 2);
    LfoSync (5, 0.9f, 3, 1.0f);
    Gate (0.80f, 3, 3);
    Filt (1, 2200, 3.5f);
    DlyMs (160, 0.72f, 0.45f);
    Env (0, { {0,0,0}, {0.01f,1,0}, {1.5f,0,-0.2f} }, 1);
    save ("FX Zloty Skurwysyn");

    // ================================================================
    // RV · REVERB SPACES (6)
    // ================================================================

    // Katedra: drawbar+srebrny triangle w wielkiej przestrzeni.
    init (0.52f);
    O (1, 7, 0.60f, 0, 0, 1.4f);
    O (2, 2, 0.45f, 0.10f, 4, 1.5f);
    P ("unison", 0.30f); P ("spread", 1.0f);
    Filt (0, 3600, 0.6f);
    Dly (1, 0.25f, 0.10f);
    FX (0, 0.12f, 0, 0.55f, 1.0f);
    AmpADSR (0.10f, 0.2f, 0.9f, 2.5f);
    save ("RV Golden Cathedral");

    // Przestrzeń zamiast nuty: maksymalny pogłos, widmo płynie wolno.
    init (0.50f);
    O (1, 0, 0.55f);
    O (2, 9, 0.45f, 0.20f, 0, 1.4f);
    O (3, 12, 0.35f, 0, 0, 1.6f);
    P ("spread", 1.0f);
    Filt (0, 3000, 0.7f);
    Lfo (0.07f, 0.25f, 3, 0.6f);
    Mod (1, 5, 0.35f); EnvRise (1, 8.0f);
    Mod (2, 9, -0.35f); EnvRise (2, 11.0f);
    FX (0, 0.10f, 0, 0.70f, 1.0f);
    AmpPad (3.5f, 8.0f);
    save ("RV Infinite Phi Space");

    // Jaskinia z gongiem: niski brąz odbija się w ciemnej grocie.
    init (0.50f);
    O (1, 2, 0.60f, 0.70f, 5, 1.8f);
    O (2, 0, 0.45f, 0, 0, 1.0f, 1);
    P ("ringMix", 0.30f); P ("subLevel", 0.30f); P ("spread", 0.80f);
    Filt (0, 1800, 0.8f);
    Dly (0, 0.30f, 0.10f);
    FX (0.10f, 0.20f, 0, 0.50f, 1.0f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {4.0f,0.2f,-0.5f}, {9.0f,0,-0.4f} }, 3);
    save ("RV Bronze Cavern");

    // Mroźna szklana hala: jasny fold przed pogłosem, mały damping w głowie.
    init (0.52f);
    O (1, 0, 0.70f);
    O (2, 9, 0.50f, 0.25f, 0, 0.8f);
    P ("fmAmt", 0.10f); P ("spread", 1.0f);
    Filt (2, 500, 0.8f);
    Mod (1, 8, 0.30f); EnvFall (1, 0.07f);
    FX (0, 0, 0.18f, 0.50f, 0.95f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {3.0f,0.2f,-0.5f}, {6.0f,0,-0.4f} }, 3);
    save ("RV Frozen Glass Hall");

    // Niebo chóralne: trzy drawbary, unison, pogłos i złote echo.
    init (0.52f);
    O (1, 7, 0.60f, 0.10f, 3, 1.5f);
    O (2, 7, 0.45f, 0.10f, 3, 1.5f, 2, 6);
    O (3, 7, 0.35f, 0, 0, 1.4f, 2, -6);
    P ("unison", 0.50f); P ("spread", 1.0f);
    Filt (0, 3200, 0.6f);
    Dly (1, 0.30f, 0.16f);
    FX (0, 0.15f, 0.08f, 0.58f, 0.95f);
    AmpPad (1.0f, 5.0f);
    save ("RV Phyllo Choir Heaven");

    // Odwrócony wszechświat: nuta "zasysa się" (wolny atak, widmo jaśnieje),
    // po puszczeniu rozpada w pogłosie.
    init (0.52f);
    O (1, 3, 0.55f, 0, 0, 2.4f);
    O (2, 9, 0.45f, 0, 0, 2.0f);
    P ("spread", 0.90f);
    Filt (0, 2200, 0.8f);
    Mod (1, 9, -0.55f); EnvRise (1, 4.0f);
    Mod (2, 5, 0.45f); EnvRise (2, 5.0f);
    Mod (3, 8, 0.35f); EnvRise (3, 6.0f);
    FX (0, 0.12f, 0, 0.65f, 1.0f);
    AmpPad (4.5f, 6.0f);
    save ("RV Reversed Universe");

    // ================================================================
    // DI · DESTROYED (6)
    // ================================================================

    // Stare elektro-mechaniczne pianino z uszkodzonym przetwornikiem.
    init (0.52f);
    O (1, 3, 0.60f, 0.18f, 6, 1.5f);
    O (2, 0, 0.45f);
    O (3, 9, 0.35f, 0, 0, 0.9f);
    P ("spread", 0.25f);
    Filt (0, 3500, 0.8f);
    Lfo (0.25f, 0.20f, 3, 0.3f);
    Mod (1, 11, -0.55f); EnvFall (1, 0.10f);
    FX (0.15f, 0.30f, 0.15f, 0.15f, 0.4f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {0.4f,0.4f,-0.4f}, {1.6f,0.15f,-0.35f},
              {3.0f,0,-0.3f} }, 4);
    save ("DI Broken Golden Piano");

    // Zardzewiała kalimba: brąz w widmie plus brud w efektorze.
    init (0.58f);
    O (1, 9, 0.80f, 0.10f, 5, 1.1f);
    O (2, 0, 0.40f);
    P ("ringMix", 0.08f); P ("spread", 0.40f);
    Filt (0, 5000, 0.9f);
    FX (0.15f, 0.25f, 0.08f, 0.20f, 0.4f);
    AmpPerc (0.002f, 0.5f);
    save ("DI Rusted Kalimba");

    // Nadpalona pozytywka gra sama — Random φ przez przester i ciemny pogłos.
    init (0.55f, 88);
    O (1, 0, 0.60f);
    O (2, 10, 0.55f, 0, 1, 0.8f, 4);
    P ("spread", 0.50f);
    Filt (0, 9000, 0.7f);
    Arp (2, 4, 4, true, 0.65f);
    Dly (1, 0.40f, 0.25f);
    FX (0.25f, 0.10f, 0.15f, 0.35f, 0.8f);
    AmpPerc (0.002f, 0.7f);
    save ("DI Burned Music Box");

    // Skorodowane smyczki: przester PRZED pogłosem — przestrzeń niesie rdzę.
    init (0.48f);
    O (1, 3, 0.55f, 0.12f, 3, 1.7f);
    O (2, 8, 0.50f, 0, 0, 1.6f);
    O (3, 12, 0.40f, 0, 0, 1.8f);
    P ("unison", 0.50f); P ("spread", 0.90f);
    Filt (0, 2200, 0.8f);
    FX (0.12f, 0.35f, 0.10f, 0.40f, 0.9f);
    AmpADSR (0.6f, 0.3f, 0.85f, 1.2f);
    save ("DI Corroded Strings");

    // Radioaktywny gong: dwa metale w ringu, wszystko lekko topione.
    init (0.48f);
    O (1, 2, 0.55f, 0.60f, 5, 1.3f);
    O (2, 2, 0.50f, 0.55f, 4, 1.2f, 1);
    P ("ringMix", 0.55f); P ("fmAmt", 0.18f); P ("spread", 0.90f);
    Filt (0, 6000, 0.8f);
    Mod (1, 8, 0.40f); EnvFall (1, 0.3f);
    Dly (1, 0.30f, 0.15f);
    FX (0.25f, 0.20f, 0.30f, 0.40f, 0.95f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {3.5f,0.25f,-0.5f}, {8.0f,0,-0.4f} }, 3);
    save ("DI Radioactive Gong");

    // Przeciążony radiowęzeł: wąskie pasmo, hard clip, krótki brudny room.
    init (0.45f);
    O (1, 1, 0.65f, 0, 0, 0.8f);
    O (2, 7, 0.50f, 0, 0, 0.9f);
    O (3, 5, 0.12f);
    P ("spread", 0.05f);
    Filt (1, 900, 1.8f);
    FX (0.65f, 0.20f, 0.25f, 0.06f, 0.25f);
    AmpADSR (0.003f, 0.2f, 0.8f, 0.1f);
    save ("DI Broken Speaker");

    // ================================================================
    // PH · PHI STUDIES (13) — duch maszyny
    // Etiudy: każda izoluje JEDEN złoty mechanizm w laboratoryjnej czystości.
    // Numeracja = kolejność kursu: wysokość → widmo → stereo → rytm → czas.
    // ================================================================

    // Sam interwał: sinusy w 0 / +φ / +2φ (833¢). φ jako WYSOKOŚĆ.
    init (0.60f);
    O (1, 0, 0.70f);
    O (2, 0, 0.50f, 0, 0, 1.0f, 3);
    O (3, 0, 0.30f, 0, 0, 1.0f, 4);
    P ("spread", 0.20f);
    Filt (0, 12000, 0.3f);
    AmpADSR (0.01f, 0.05f, 0.95f, 0.3f);
    save ("PH 01 Pure 833");

    // Jeden saw wędruje z szeregu harmonicznego do f·φⁿ i wraca. φ jako WIDMO.
    init (0.55f);
    O (1, 3, 0.80f, 0, 0, 1.2f);
    P ("spread", 0.40f);
    Filt (0, 8000, 0.5f);
    Mod (1, 5, 1.0f); EnvSwell (1, 6.0f, 8.0f);
    AmpADSR (0.05f, 0.1f, 0.9f, 0.5f);
    save ("PH 02 Spiral Unwinds");

    // Partiale rozsiane kątem złotym: fundament w centrum, góra kwitnie
    // na boki w miarę rozjaśniania. φ jako STEREO.
    init (0.55f);
    O (1, 3, 0.65f, 0, 0, 1.6f);
    P ("spread", 1.0f);
    Filt (0, 12000, 0.4f);
    Mod (1, 9, -0.50f); EnvRise (1, 4.0f);
    AmpPad (1.0f, 3.0f);
    save ("PH 03 Golden Angle");

    // Słowo Fibonacciego tnie ciągły ton na S/L — bez niczego więcej.
    // Fibonacci jako RYTM.
    init (0.60f, 120);
    O (1, 2, 0.70f, 0, 0, 1.2f);
    O (2, 0, 0.40f);
    Filt (0, 6000, 0.6f);
    Gate (1.0f, 2, 2);
    AmpADSR (0.005f, 0.05f, 0.95f, 0.15f);
    save ("PH 04 The Word");

    // Schodki Weyla frac(k/φ) na cutoffie: "random", który nigdy nie wraca
    // i równo pokrywa zakres. φ jako LOSOWOŚĆ.
    init (0.55f, 110);
    O (1, 3, 0.75f, 0, 0, 1.3f);
    Filt (0, 800, 2.5f);
    LfoSync (3, 0.80f, 3);
    AmpADSR (0.005f, 0.1f, 0.9f, 0.2f);
    save ("PH 05 Weyl Stairs");

    // Krótki blip w Golden Delay: 4 tapy w t/φ³..t — słychać złote,
    // niewspółmierne odstępy ech. φ jako CZAS.
    init (0.65f);
    O (1, 0, 0.80f);
    P ("spread", 0.30f);
    Filt (0, 12000, 0.4f);
    DlyMs (500, 0.55f, 0.60f);
    AmpPerc (0.001f, 0.06f);
    save ("PH 06 Echo of Phi");

    // Czysty sinus, modulator f·φ pęcznieje i gaśnie — sidebandy kwitną
    // w nieharmonicznych miejscach. φ jako ENERGIA (FM).
    init (0.60f);
    O (1, 0, 0.85f);
    Filt (0, 10000, 0.4f);
    Mod (1, 8, 0.60f); EnvSwell (1, 3.0f, 4.0f);
    AmpADSR (0.05f, 0.1f, 0.9f, 0.5f);
    save ("PH 07 Sidebands Bloom");

    // Ring sin(f)·sin(f·φ): produkt daje f(φ+1)=f·φ² oraz f(φ−1)=f/φ —
    // złote TOŻSAMOŚCI słyszalne jako czysty dwudźwięk.
    init (0.60f);
    O (1, 0, 0.25f);
    O (2, 0, 0.25f, 0, 0, 1.0f, 3);
    P ("ringMix", 1.0f); P ("spread", 0.30f);
    Filt (0, 12000, 0.3f);
    AmpADSR (0.01f, 0.05f, 0.95f, 0.3f);
    save ("PH 08 Identity Ring");

    // Sub-partial na f/φ pod czystym sinusem: złota "sub-oktawa" −833¢.
    init (0.62f);
    O (1, 0, 0.70f);
    P ("subLevel", 1.0f); P ("spread", 0);
    Filt (0, 8000, 0.3f);
    AmpADSR (0.01f, 0.05f, 0.95f, 0.3f);
    save ("PH 09 Under the Root");

    // Golden Unison solo: bliźniaki rozstrojone frac(n·φ) — chór, który
    // nigdy nie zapętla fazy (low-discrepancy zamiast LFO).
    init (0.58f);
    O (1, 3, 0.70f, 0, 0, 1.3f);
    P ("unison", 1.0f); P ("spread", 0.50f);
    Filt (0, 5000, 0.6f);
    AmpPad (0.3f, 2.0f);
    save ("PH 10 Never Twice");

    // Grają tylko partiale o numerach Fibonacciego (1,2,3,5,8,13) — obok
    // cichy grzebień Lucasa dla kontrastu. Króliki Fibonacciego jako WIDMO.
    init (0.62f);
    O (1, 10, 0.80f, 0, 0, 1.0f);
    O (2, 11, 0.35f, 0, 0, 1.1f);
    P ("spread", 0.60f);
    Filt (0, 12000, 0.4f);
    Dly (1, 0.30f, 0.20f);
    AmpPerc (0.002f, 1.2f);
    save ("PH 11 Rabbit Harp");

    // Kaskada 1/φᵏ: każdy segment obwiedni krótszy od poprzedniego o φ —
    // samopodobny decay jak struna, która gaśnie "fraktalnie".
    init (0.65f);
    O (1, 9, 0.85f, 0, 0, 1.3f);
    P ("spread", 0.35f);
    Filt (0, 9000, 0.5f);
    AmpCasc (0.003f, 0.62f);
    save ("PH 12 Self-Similar Fall");

    // Finał kursu: cała maszyna naraz, ale muzycznie — 833¢ w harmonii,
    // słowo w rytmie, Weyl w dynamice, φ w echu i stereo.
    init (0.55f, 112);
    O (1, 3, 0.55f, 0.10f, 3, 1.5f);
    O (2, 0, 0.40f, 0, 0, 1.0f, 3);
    O (3, 9, 0.35f, 0, 0, 1.2f);
    P ("subLevel", 0.30f); P ("unison", 0.40f); P ("spread", 0.90f);
    Filt (0, 4500, 0.8f);
    Lfo (0.25f, 0.25f, 3, 0.5f);
    Arp (2, 4, 6, true, 0.50f);
    Gate (0.30f, 2, 2);
    Dly (1, 0.45f, 0.30f);
    FX (0, 0.10f, 0, 0.25f, 0.7f);
    AmpPerc (0.003f, 0.5f);
    save ("PH 13 Omega Machine");

    std::printf ("\n%d presetow, %d bledow\nkatalog: %s\n", count, failures,
                 FiSynthAudioProcessor::getPresetDirectory().getFullPathName().toRawUTF8());
    return failures;
}
