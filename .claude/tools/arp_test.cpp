// Test trybów arpeggiatora: Up rośnie, Down maleje, Up-Down Ex zawraca,
// Random φ trzyma się dozwolonego zbioru interwałów.
#include <JuceHeader.h>
#include <cstdio>
#include <vector>
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

// Dominująca częstotliwość segmentu (FFT 8192, okno Hanna).
static double domFreq (const float* x, int n, double sr)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf (2 * N, 0.0f);
    for (int i = 0; i < N && i < n; ++i)
        buf[(size_t) i] = x[i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1)));
    fft.performRealOnlyForwardTransform (buf.data());
    int best = 1; double bestA = 0;
    for (int k = 4; k < N / 2; ++k)
    {
        const double a = std::hypot ((double) buf[(size_t)(2*k)], (double) buf[(size_t)(2*k+1)]);
        if (a > bestA) { bestA = a; best = k; }
    }
    return best * sr / N;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    };

    constexpr double sr = 48000.0;
    constexpr int blk = 512;

    // Kroki 1/8 przy 120 BPM = 0.25 s = 12000 próbek. Zwraca f0 (i opcjonalnie
    // RMS) drugiej połowy każdego PODZIAŁU.
    std::vector<double> lastRms;
    auto runMode = [&] (int mode, int numSteps, bool word = false,
                        float vel = 0.0f) -> std::vector<double>
    {
        std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
        proc->setPlayConfigDetails (0, 2, sr, blk);
        proc->prepareToPlay (sr, blk);
        setParam (*proc, "osc1waveform", 0);   // sinus — czyste f0
        setParam (*proc, "osc1mix", 1.0f);
        setParam (*proc, "osc2mix", 0.0f);
        setParam (*proc, "osc3mix", 0.0f);
        setParam (*proc, "filterCutoff", 20000.0f);
        setParam (*proc, "lfoDepth", 0.0f);
        setParam (*proc, "bpm", 120.0f);
        setParam (*proc, "arpOn", 1.0f);
        setParam (*proc, "arpDiv", 1.0f);      // 1/8
        setParam (*proc, "arpLen", 2.0f);      // 3 kroki (offsety 0,1,2)
        setParam (*proc, "arpMode", (float) mode);
        setParam (*proc, "arpWord", word ? 1.0f : 0.0f);
        setParam (*proc, "arpVel", vel);

        const int stepSamples = 12000;
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, blk);
        const int blocks = (numSteps * stepSamples) / blk + 4;
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            proc->processBlock (buf, midi);
            for (int i = 0; i < blk; ++i)
                out.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
        }

        // f0 + RMS z drugiej połowy każdego podziału (po ataku obwiedni).
        std::vector<double> freqs;
        lastRms.clear();
        for (int st = 0; st < numSteps; ++st)
        {
            const float* seg = out.data() + st * stepSamples + stepSamples / 3;
            const int    n   = stepSamples / 2;
            freqs.push_back (domFreq (seg, n, sr));
            double acc = 0;
            for (int i = 0; i < n; ++i) acc += (double) seg[i] * seg[i];
            lastRms.push_back (std::sqrt (acc / n));
        }
        return freqs;
    };

    auto semis = [] (double fLo, double fHi) { return 12.0 * std::log2 (fHi / fLo); };

    {   // Up: 0,1,2 — rosnąco po ~1 półtonie
        const auto f = runMode (0, 3);
        check (semis (f[0], f[1]) > 0.5 && semis (f[1], f[2]) > 0.5,
               "Up: kroki rosnaco (" + juce::String (f[0],0) + " " + juce::String (f[1],0)
                   + " " + juce::String (f[2],0) + " Hz)");
    }
    {   // Down: 2,1,0 — malejąco
        const auto f = runMode (1, 3);
        check (semis (f[1], f[0]) > 0.5 && semis (f[2], f[1]) > 0.5,
               "Down: kroki malejaco (" + juce::String (f[0],0) + " " + juce::String (f[1],0)
                   + " " + juce::String (f[2],0) + " Hz)");
    }
    {   // Up-Down Ex (len 3, okres 4): 0,1,2,1 — czwarty krok schodzi
        const auto f = runMode (3, 4);
        check (semis (f[0], f[1]) > 0.5 && semis (f[1], f[2]) > 0.5 && semis (f[3], f[2]) > 0.5,
               "Up-Down Ex: zawraca po szczycie");
    }
    {   // Random φ: każdy krok w dozwolonym zbiorze {0,+1,+2} półtonów od C4
        const auto f = runMode (4, 6);
        const double f0 = juce::MidiMessage::getMidiNoteInHertz (60);
        bool ok = true;
        for (double fr : f)
        {
            const double st = 12.0 * std::log2 (fr / f0);
            const double frac = std::abs (st - std::round (st));
            ok = ok && frac < 0.2 && std::round (st) >= 0 && std::round (st) <= 2;
        }
        check (ok, "Random fi: nuty w zbiorze {0,+1,+2} od roota");
    }

    {   // Fib Walk (len 3): pozycje (F(k+2)−1) mod 3 = 0,1,2,1,1,0…
        const auto f = runMode (6, 5);
        const double f0 = juce::MidiMessage::getMidiNoteInHertz (60);
        auto st = [&] (double fr) { return (int) std::lround (12.0 * std::log2 (fr / f0)); };
        const bool ok = st (f[0]) == 0 && st (f[1]) == 1 && st (f[2]) == 2
                     && st (f[3]) == 1 && st (f[4]) == 1;
        check (ok, "Fib Walk: sekwencja 0,1,2,1,1 (Pisano)");
    }
    {   // Word: podziały-nuty wg SLSS… -> pitch per podział: 0,1,1,2,0,1,1
        const auto f = runMode (0, 7, true);
        const double f0 = juce::MidiMessage::getMidiNoteInHertz (60);
        auto st = [&] (double fr) { return (int) std::lround (12.0 * std::log2 (fr / f0)); };
        const bool ok = st (f[0]) == 0 && st (f[1]) == 1 && st (f[2]) == 1
                     && st (f[3]) == 2 && st (f[4]) == 0 && st (f[5]) == 1 && st (f[6]) == 1;
        check (ok, "Word: L trwa 2 podzialy (wzor 0,1,1,2,0,1,1)");
    }
    {   // Vel φ: przy pełnej głębokości RMS kroków wyraźnie się różni
        (void) runMode (0, 6, false, 1.0f);
        double lo = 1e9, hi = 0;
        for (double r : lastRms) { lo = juce::jmin (lo, r); hi = juce::jmax (hi, r); }
        check (hi / juce::jmax (1.0e-12, lo) > 1.3,
               "Vel fi: dynamika krokow zroznicowana (max/min=" + juce::String (hi / lo, 2) + ")");
    }

    std::printf (failures == 0 ? "\nWSZYSTKIE TESTY OK\n" : "\n%d TESTOW PADLO\n", failures);
    return failures;
}
