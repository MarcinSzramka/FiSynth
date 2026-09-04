// Przemiata CALY bank presetow: kazdy .fsynth wczytany, zagrany tym samym
// akordem i zrzucony do jednego pliku float32. Dwa przebiegi (przed/po zmianie
// w silniku) porownuje compare_raw.py — to najszersza siec na regresje brzmienia,
// bo bank pokrywa wszystkie waveformy, tryby stretcha i kombinacje zlotego silnika,
// ktorych syntetyczny benchmark nie dotyka.
//
//   preset_sweep <katalog-presetow> <wyjscie.raw>
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <vector>
#include <cmath>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 3)
    {
        std::printf ("uzycie: preset_sweep <katalog> <wyjscie.raw>\n");
        return 2;
    }

    juce::File dir { juce::String (argv[1]) };
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.fsynth");
    files.sort();

    if (files.isEmpty())
    {
        std::printf ("brak presetow w %s\n", dir.getFullPathName().toRawUTF8());
        return 2;
    }

    constexpr double sr = 48000.0;
    constexpr int    blockSize = 128;
    constexpr double seconds = 0.6;              // krotko, ale przez atak i sustain
    const int totalBlocks = (int) (sr * seconds / blockSize);

    // Akord pokrywajacy szeroki rejestr — nizsze nuty pokazuja sub/unison,
    // wyzsze wypychaja partiale ku Nyquistowi (tam dziala `break`).
    const int notes[] = { 36, 52, 67, 79 };

    std::vector<float> dump;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    proc->setPlayConfigDetails (0, 2, sr, blockSize);
    proc->prepareToPlay (sr, blockSize);

    auto* fi = dynamic_cast<FiSynthAudioProcessor*> (proc.get());
    if (fi == nullptr) { std::printf ("zly typ procesora\n"); return 2; }

    int loaded = 0, silent = 0, nonFinite = 0;
    float globalPeak = 0.0f;

    for (const auto& f : files)
    {
        if (! fi->loadPreset (f))
        {
            std::printf ("  [SKIP] nie wczytal: %s\n", f.getFileName().toRawUTF8());
            continue;
        }
        ++loaded;

        // Czysty start: wycisz wszystko, co zostalo po poprzednim presecie.
        proc->reset();

        juce::AudioBuffer<float> buf (2, blockSize);
        double sumSq = 0.0; long long cnt = 0;

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

            buf.clear();
            proc->processBlock (buf, midi);

            const auto* L = buf.getReadPointer (0);
            const auto* R = buf.getReadPointer (1);
            for (int i = 0; i < blockSize; ++i)
            {
                dump.push_back (L[i]);
                dump.push_back (R[i]);
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ++nonFinite;
                else globalPeak = juce::jmax (globalPeak, std::abs (L[i]));
                sumSq += (double) L[i] * L[i];
                ++cnt;
            }
        }

        const double rms = std::sqrt (sumSq / (double) juce::jmax (1LL, cnt));
        if (rms < 1.0e-5) { ++silent; std::printf ("  [CISZA] %s\n", f.getFileName().toRawUTF8()); }
    }

    juce::File out { juce::String (argv[2]) };
    out.replaceWithData (dump.data(), dump.size() * sizeof (float));

    std::printf ("\npresety: %d wczytanych / %d w katalogu\n", loaded, files.size());
    std::printf ("ciche: %d | probki nieskonczone: %d | peak: %.4f\n", silent, nonFinite, globalPeak);
    std::printf ("zapisano %zu probek -> %s\n", dump.size(), out.getFullPathName().toRawUTF8());
    return (nonFinite > 0) ? 1 : 0;
}
