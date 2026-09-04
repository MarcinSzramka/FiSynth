// Benchmark + snapshot brzmienia gorącej pętli syntezy (sekcja B audytu).
//
//   bench_voice <plik.raw>
//
// Renderuje kilka patchy o różnym profilu kosztu, mierzy czas renderowania
// (jako % jednego rdzenia przy realnym czasie audio) i zapisuje surowe float32
// (interleaved stereo) do <plik.raw>. Porównanie dwóch przebiegów robi
// compare_raw.py — dzięki temu każdą optymalizację widać i w CPU, i w błędzie
// względem referencji.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <chrono>
#include <cmath>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

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

struct Patch
{
    const char* name;
    void (*apply) (juce::AudioProcessor&);
};

// Patch domyślny: to, co słychać zaraz po wczytaniu pluginu.
static void patchDefault (juce::AudioProcessor&) {}

// "Golden": wszystkie kosztowne dodatki złotego silnika naraz.
static void patchGolden (juce::AudioProcessor& p)
{
    setParam (p, "unison",  0.8f);
    setParam (p, "ringMix", 0.5f);
    setParam (p, "subLevel", 0.4f);
    setParam (p, "fmAmt",   0.3f);
    setParam (p, "spread",  0.7f);
}

// Stretch: nieharmoniczne ratio => część partiali ląduje nad Nyquistem.
static void patchStretch (juce::AudioProcessor& p)
{
    setParam (p, "osc1stretch", 1.0f);
    setParam (p, "osc1stretchmode", 1.0f);
    setParam (p, "osc2stretch", 0.7f);
    setParam (p, "osc2mix", 0.8f);
    setParam (p, "osc3stretch", 0.5f);
    setParam (p, "osc3mix", 0.8f);
    setParam (p, "spread", 0.5f);
}

static const Patch patches[] = {
    { "default", patchDefault },
    { "golden",  patchGolden  },
    { "stretch", patchStretch },
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String outPath = argc > 1 ? argv[1] : "bench.raw";

    constexpr double sr = 48000.0;
    constexpr int    blockSize = 128;
    constexpr double seconds = 4.0;
    const int totalBlocks = (int) (sr * seconds / blockSize);

    // 8 głosów naraz — pełne obciążenie, tak jak w audycie.
    const int notes[] = { 36, 43, 48, 52, 55, 60, 64, 67 };

    std::vector<float> dump;
    dump.reserve ((size_t) (totalBlocks * blockSize * 2));

    std::printf ("%-9s %10s %10s %12s\n", "patch", "czas[ms]", "%rdzenia", "RMS");

    for (const auto& patch : patches)
    {
        std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
        p->setPlayConfigDetails (0, 2, sr, blockSize);
        p->prepareToPlay (sr, blockSize);
        patch.apply (*p);

        juce::AudioBuffer<float> buf (2, blockSize);
        double sumSq = 0.0; long long count = 0;

        // Rozgrzewka: 8 nut wchodzi w pierwszych blokach, potem tylko render.
        const auto t0 = std::chrono::steady_clock::now();

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b < 8)
                midi.addEvent (juce::MidiMessage::noteOn (
                    1, notes[b], (juce::uint8) 100), 0);

            buf.clear();
            p->processBlock (buf, midi);

            const auto* L = buf.getReadPointer (0);
            const auto* R = buf.getReadPointer (1);
            for (int i = 0; i < blockSize; ++i)
            {
                dump.push_back (L[i]);
                dump.push_back (R[i]);
                sumSq += (double) L[i] * L[i];
                ++count;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli> (t1 - t0).count();
        const double pct = ms / (seconds * 1000.0) * 100.0;
        const double rms = std::sqrt (sumSq / (double) juce::jmax (1LL, count));

        std::printf ("%-9s %10.1f %10.2f %12.6f\n", patch.name, ms, pct, rms);
    }

    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    f.replaceWithData (dump.data(), dump.size() * sizeof (float));
    std::printf ("\nzapisano %zu probek -> %s\n", dump.size(), f.getFullPathName().toRawUTF8());
    return 0;
}
