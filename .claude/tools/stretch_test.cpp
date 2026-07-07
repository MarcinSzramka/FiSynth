// Test DSP fixów Fazy 1:
// 1) zmiana stretcha W TRAKCIE trzymanej nuty zmienia widmo (odmrożenie delt),
// 2) CC123 (All Notes Off) przy włączonym arpie daje trwałą ciszę (arpHeld czyszczone).
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
    std::printf ("BRAK PARAMETRU %s\n", id.toRawUTF8());
}

// Energia widma powyżej progu Hz (FFT 16384 na końcówce sygnału).
static double highBandEnergy (const std::vector<float>& x, double sr, double fromHz)
{
    constexpr int order = 14, N = 1 << order;
    if ((int) x.size() < N) return 0.0;

    juce::dsp::FFT fft (order);
    std::vector<float> buf (2 * N, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1));
        buf[(size_t) i] = x[x.size() - N + (size_t) i] * w;
    }
    fft.performRealOnlyForwardTransform (buf.data());

    double e = 0.0;
    const int fromBin = (int) (fromHz / sr * N);
    for (int k = fromBin; k < N / 2; ++k)
    {
        const float re = buf[(size_t) (2 * k)], im = buf[(size_t) (2 * k + 1)];
        e += (double) re * re + (double) im * im;
    }
    return e;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    };

    constexpr double sr = 48000.0;
    constexpr int blk = 512;

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    proc->setPlayConfigDetails (0, 2, sr, blk);
    proc->prepareToPlay (sr, blk);

    setParam (*proc, "osc1waveform", 3.0f);   // saw — dużo partiali
    setParam (*proc, "osc1mix", 1.0f);
    setParam (*proc, "osc2mix", 0.0f);
    setParam (*proc, "osc3mix", 0.0f);
    setParam (*proc, "osc1stretch", 0.0f);
    setParam (*proc, "osc1stretchmode", 0.0f); // Golden
    setParam (*proc, "filterCutoff", 20000.0f);
    setParam (*proc, "lfoDepth", 0.0f);

    juce::AudioBuffer<float> buf (2, blk);
    std::vector<float> tail;

    auto render = [&] (int blocks, juce::MidiBuffer* firstBlockMidi)
    {
        tail.clear();
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0 && firstBlockMidi != nullptr)
                midi = *firstBlockMidi;
            proc->processBlock (buf, midi);
            for (int i = 0; i < blk; ++i)
                tail.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
        }
    };

    // === Test 1: stretch zmieniany w trakcie trzymanej nuty ===
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
    render (60, &on);                                   // nuta gra, stretch = 0
    const double hiA = highBandEnergy (tail, sr, 3000.0);

    setParam (*proc, "osc1stretch", 1.0f);              // drag spirali mid-note
    render (60, nullptr);                               // nuta wciąż trzymana
    const double hiB = highBandEnergy (tail, sr, 3000.0);

    std::printf ("  energia >3kHz: stretch0=%.3g  stretch1=%.3g  (x%.1f)\n",
                 hiA, hiB, hiB / juce::jmax (1.0e-12, hiA));
    check (hiB > hiA * 4.0, "stretch zmienia widmo TRZYMANEJ nuty (delty odmrozone)");

    // Sprzątanie po teście 1.
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
    render (100, &off);

    // === Test 2: panic (CC123) przy włączonym arpie ===
    setParam (*proc, "arpOn", 1.0f);
    render (40, &on);                                   // arp gra od roota 48
    double rmsPlaying = 0.0;
    for (float s : tail) rmsPlaying += (double) s * s;
    rmsPlaying = std::sqrt (rmsPlaying / (double) tail.size());
    check (rmsPlaying > 1.0e-4, "arp gra przy trzymanym klawiszu");

    juce::MidiBuffer panic;
    panic.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    render (280, &panic);                               // ~3 s na release
    const size_t lastN = (size_t) (0.25 * sr);
    double rmsAfter = 0.0;
    for (size_t i = tail.size() - lastN; i < tail.size(); ++i)
        rmsAfter += (double) tail[i] * tail[i];
    rmsAfter = std::sqrt (rmsAfter / (double) lastN);
    std::printf ("  rms w trakcie=%.5f  po panic=%.7f\n", rmsPlaying, rmsAfter);
    check (rmsAfter < 1.0e-5, "CC123 ucisza arp NA STALE (arpHeld wyczyszczone)");

    std::printf (failures == 0 ? "\nWSZYSTKIE TESTY OK\n" : "\n%d TESTOW PADLO\n", failures);
    return failures;
}
