// Testy sekcji A audytu:
//  1. stabilność filtra przy sample rate < 40 kHz (clamp cutoffa do Nyquista),
//  2. bezpiecznik NaN/Inf (nanResetCount + skończone wyjście),
//  3. declick przy kradzieży głosu (brak skoku między próbkami przy 9. nucie).
//
// Uruchamiać na zbudowanej libFiSynth_SharedCode.a — patrz recipe w .claude.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int failures = 0;

static void check (bool ok, const char* what, const juce::String& detail = {})
{
    std::printf ("  [%s] %s%s\n", ok ? " OK " : "FAIL", what,
                 detail.isEmpty() ? "" : (" — " + detail).toRawUTF8());
    if (! ok)
        ++failures;
}

static void setParam (juce::AudioProcessor& p, const juce::String& id, float plain)
{
    for (auto* par : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par))
            if (rp->paramID == id)
            {
                rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
                return;
            }
}

// Najbardziej wymagający patch dla filtra: cutoff na maksa, rezonans na maksa,
// pełna modulacja LFO w górę. Dokładnie to, co przy sr 32 kHz rozbijało filtr.
static void extremePatch (juce::AudioProcessor& p)
{
    setParam (p, "filterCutoff", 20000.0f);
    setParam (p, "filterResonance",    10.0f);
    setParam (p, "filterType",   0.0f);
    setParam (p, "lfoDepth",     1.0f);
    setParam (p, "lfoRate",      8.0f);
    setParam (p, "gain",         0.8f);
}

struct RenderStats
{
    bool  allFinite { true };
    float peak { 0.0f };
    float maxStep { 0.0f };   // największy skok |x[n] - x[n-1]|
};

// Renderuje bloki, licząc statystyki. midiPerBlock woła się przed każdym blokiem.
template <typename MidiFn>
static RenderStats render (juce::AudioProcessor& p, int numBlocks, int blockSize,
                           MidiFn&& midiPerBlock)
{
    juce::AudioBuffer<float> buf (2, blockSize);
    RenderStats st;
    float prev = 0.0f;
    bool  havePrev = false;

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::MidiBuffer midi;
        midiPerBlock (b, midi);

        buf.clear();
        p.processBlock (buf, midi);

        const auto* L = buf.getReadPointer (0);
        for (int i = 0; i < blockSize; ++i)
        {
            const float v = L[i];
            if (! std::isfinite (v))
            {
                st.allFinite = false;
                prev = 0.0f; havePrev = false;
                continue;
            }
            st.peak = juce::jmax (st.peak, std::abs (v));
            if (havePrev)
                st.maxStep = juce::jmax (st.maxStep, std::abs (v - prev));
            prev = v; havePrev = true;
        }
    }
    return st;
}

static std::unique_ptr<juce::AudioProcessor> makeProc (double sr, int blockSize)
{
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->setPlayConfigDetails (0, 2, sr, blockSize);
    p->prepareToPlay (sr, blockSize);
    return p;
}

// --- Test 1 + 2: stabilność filtra i bezpiecznik NaN przy różnych sample rate ---
static void testFilterStability()
{
    std::printf ("\n== Test 1/2: stabilnosc filtra + bezpiecznik NaN ==\n");

    for (double sr : { 22050.0, 32000.0, 44100.0, 48000.0 })
    {
        constexpr int blockSize = 128;
        auto p = makeProc (sr, blockSize);
        extremePatch (*p);

        // 1 sekunda trzymanej nuty — rozbieg filtra dawał +60 dB w ~6 ms,
        // więc ten czas jest o rzędy wielkości dłuższy niż potrzebny.
        const int blocks = (int) (sr * 1.0 / blockSize);
        auto st = render (*p, blocks, blockSize, [] (int b, juce::MidiBuffer& m)
        {
            if (b == 2)
                m.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 110), 0);
        });

        auto* fi = dynamic_cast<FiSynthAudioProcessor*> (p.get());
        const int nanHits = fi != nullptr ? fi->nanResetCount.load() : -1;

        const juce::String tag ("sr " + juce::String ((int) sr));
        check (st.allFinite, (tag + ": wyjscie skonczone").toRawUTF8());
        check (st.peak < 4.0f, (tag + ": brak rozbiegu").toRawUTF8(),
               "peak " + juce::String (st.peak, 3));
        check (nanHits == 0, (tag + ": bezpiecznik NaN nie musial dzialac").toRawUTF8(),
               "nanResetCount " + juce::String (nanHits));
    }
}

// --- Test 3: declick przy kradzieży głosu ---
// 8 głosów; 9. i kolejne nuty MUSZĄ ukraść brzmiący głos. Mierzymy skok DOKŁADNIE
// na granicy kradzieży (ostatnia próbka bloku przed noteOn vs pierwsza próbka bloku
// z noteOn) — w obu buildach to te same dwie próbki, więc porównanie nie jest
// zaszumione naturalnym nachyleniem sygnału jak metryka "max po całej fazie".
// Przed poprawką obwiednia skradzionego głosu spadała do zera w JEDNEJ próbce.
static void testStealDeclick()
{
    std::printf ("\n== Test 3: declick przy kradziezy glosu ==\n");

    constexpr double sr = 48000.0;
    constexpr int blockSize = 64;
    auto p = makeProc (sr, blockSize);

    setParam (*p, "gain", 0.8f);
    setParam (*p, "filterCutoff", 12000.0f);
    setParam (*p, "filterResonance", 1.0f);

    const int blocksPerNote = (int) (sr * 0.03 / blockSize);
    juce::AudioBuffer<float> buf (2, blockSize);

    float peak = 0.0f, maxStealStep = 0.0f, maxNormalStep = 0.0f;
    float lastOfPrevBlock = 0.0f;
    bool  havePrev = false, allFinite = true;

    // 16 nut: pierwsze 8 zapełnia głosy, kolejne 8 kradną.
    for (int b = 0; b < blocksPerNote * 16; ++b)
    {
        const bool noteHere = (b % blocksPerNote == 0);
        const int  noteIdx  = b / blocksPerNote;
        const bool isSteal  = noteHere && noteIdx >= 8;

        juce::MidiBuffer midi;
        if (noteHere)
            midi.addEvent (juce::MidiMessage::noteOn (
                1, 48 + 3 * (noteIdx % 8), (juce::uint8) 100), 0);

        buf.clear();
        p->processBlock (buf, midi);
        const auto* L = buf.getReadPointer (0);

        for (int i = 0; i < blockSize; ++i)
            if (! std::isfinite (L[i])) allFinite = false;
        if (! allFinite) break;

        // Skok przez granicę bloku — tu ląduje ewentualny trzask kradzieży.
        if (havePrev)
        {
            const float step = std::abs (L[0] - lastOfPrevBlock);
            if (isSteal) maxStealStep  = juce::jmax (maxStealStep, step);
            else         maxNormalStep = juce::jmax (maxNormalStep, step);
        }

        for (int i = 0; i < blockSize; ++i)
            peak = juce::jmax (peak, std::abs (L[i]));

        lastOfPrevBlock = L[blockSize - 1];
        havePrev = true;
    }

    check (allFinite, "wyjscie skonczone podczas kradziezy");
    check (peak > 1.0e-3f, "glosy graja", "peak " + juce::String (peak, 4));

    // Odniesienie: skok na granicach bloków BEZ kradzieży to naturalne nachylenie
    // sygnału. Kradzież nie ma go istotnie przekraczać.
    const float ratio = maxNormalStep > 0.0f ? maxStealStep / maxNormalStep : 0.0f;
    std::printf ("       skok na kradziezy %.6f | skok zwykly %.6f | iloraz %.2f\n",
                 maxStealStep, maxNormalStep, ratio);
    check (ratio < 1.5f, "kradziez nie robi skoku wiekszego niz zwykle nachylenie",
           "iloraz " + juce::String (ratio, 2));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    testFilterStability();
    testStealDeclick();

    std::printf ("\n%s (%d bledow)\n", failures == 0 ? "WSZYSTKO OK" : "SA BLEDY", failures);
    return failures == 0 ? 0 : 1;
}
