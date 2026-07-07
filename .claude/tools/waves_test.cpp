// Test nowych waveformów (6..12): każdy gra, a sygnatury widmowe się zgadzają:
// Pulse 25% ma dziurę na h=4, Fib Comb wycina h=4 a zostawia h=5,
// Golden Pluck nie zeruje żadnej harmonicznej h=1..6.
#include <JuceHeader.h>
#include <cstdio>
#include <vector>

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

// Amplituda widma w okolicy freq (maksimum z ±2 binów FFT 16384).
static double binAmp (const std::vector<float>& x, double sr, double freq)
{
    constexpr int order = 14, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf (2 * N, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1));
        buf[(size_t) i] = x[x.size() - N + (size_t) i] * w;
    }
    fft.performRealOnlyForwardTransform (buf.data());

    const int centre = (int) std::lround (freq / sr * N);
    double best = 0.0;
    for (int k = juce::jmax (1, centre - 2); k <= centre + 2; ++k)
    {
        const double re = buf[(size_t) (2 * k)], im = buf[(size_t) (2 * k + 1)];
        best = juce::jmax (best, std::sqrt (re * re + im * im));
    }
    return best;
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
    const double f0 = juce::MidiMessage::getMidiNoteInHertz (48);   // C3

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    proc->setPlayConfigDetails (0, 2, sr, blk);
    proc->prepareToPlay (sr, blk);
    setParam (*proc, "osc1mix", 1.0f);
    setParam (*proc, "osc2mix", 0.0f);
    setParam (*proc, "osc3mix", 0.0f);
    setParam (*proc, "osc1stretch", 0.0f);
    setParam (*proc, "filterCutoff", 20000.0f);
    setParam (*proc, "lfoDepth", 0.0f);

    juce::AudioBuffer<float> buf (2, blk);
    std::vector<float> tail;

    auto renderWave = [&] (int wf)
    {
        setParam (*proc, "osc1waveform", (float) wf);
        tail.clear();
        for (int b = 0; b < 70; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            proc->processBlock (buf, midi);
            for (int i = 0; i < blk; ++i)
                tail.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
        }
        // zwolnij nutę i wycisz ogon przed następną falą
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
        juce::AudioBuffer<float> b2 (2, blk);
        for (int b = 0; b < 120; ++b)
        {
            juce::MidiBuffer m;
            if (b == 0) m = off;
            proc->processBlock (b2, m);
        }
    };

    static const char* names[] = { "Pulse 25%", "Drawbar", "Even", "Golden Pluck",
                                   "Fib Comb", "Lucas Comb", "Fib Word" };
    for (int wf = 6; wf <= 12; ++wf)
    {
        renderWave (wf);
        double rms = 0.0;
        for (float s : tail) rms += (double) s * s;
        rms = std::sqrt (rms / (double) tail.size());
        check (rms > 1.0e-4, juce::String (names[wf - 6]) + " gra (rms=" + juce::String (rms, 5) + ")");

        if (wf == 6)   // Pulse 25%: dziura na h=4, obecne h=3 i h=5
        {
            const double a3 = binAmp (tail, sr, 3 * f0), a4 = binAmp (tail, sr, 4 * f0),
                         a5 = binAmp (tail, sr, 5 * f0);
            check (a4 < 0.05 * juce::jmax (a3, a5),
                   "Pulse 25%: h=4 wyciete (a4/a5=" + juce::String (a4 / juce::jmax (1e-12, a5), 4) + ")");
        }
        if (wf == 10)  // Fib Comb: h=4 (nie-Fibonacci) wyciete, h=5 gra
        {
            const double a4 = binAmp (tail, sr, 4 * f0), a5 = binAmp (tail, sr, 5 * f0);
            check (a4 < 0.05 * a5,
                   "Fib Comb: h=4 wyciete, h=5 gra (a4/a5=" + juce::String (a4 / juce::jmax (1e-12, a5), 4) + ")");
        }
        if (wf == 9)   // Golden Pluck: zadna z h=1..6 nie jest zerem
        {
            double amin = 1e18, amax = 0.0;
            for (int hh = 1; hh <= 6; ++hh)
            {
                const double a = binAmp (tail, sr, hh * f0);
                amin = juce::jmin (amin, a);
                amax = juce::jmax (amax, a);
            }
            check (amin > 1.0e-4 * amax,
                   "Golden Pluck: h=1..6 wszystkie obecne (min/max=" + juce::String (amin / amax, 6) + ")");
        }
    }

    std::printf (failures == 0 ? "\nWSZYSTKIE TESTY OK\n" : "\n%d TESTOW PADLO\n", failures);
    return failures;
}
