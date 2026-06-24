#pragma once

#include <JuceHeader.h>

// Liczba niezależnych obwiedni. Indeks 0 = obwiednia amplitudy (VCA — napędza
// głośność nuty), indeksy 1..3 = obwiednie modulacyjne (każda jest źródłem dla
// odpowiadającego jej slotu modulacji env1/env2/env3 Dest+Amt).
static constexpr int kNumEnvelopes = 4;

// === Model wielopunktowej obwiedni (styl Vital/Serum) ===
//
// Obwiednia to lista punktów (czas, poziom, krzywizna segmentu) plus jeden
// wyróżniony punkt "sustain", na którym obwiednia się zatrzymuje, dopóki
// klawisz jest wciśnięty. Punkty PRZED sustainem to faza attack/decay,
// punkty PO sustainie to faza release (odgrywane po puszczeniu klawisza).
//
// Model edytowalny (EnvelopeModel) żyje na wątku komunikatów (GUI). Wątek
// audio nigdy go nie dotyka — dostaje lock-free kopię POD (EnvSnapshot),
// którą procesor podmienia pod spinlockiem.

struct EnvPoint
{
    float time  { 0.0f };  // sekundy od początku nuty (rosnąco)
    float level { 0.0f };  // 0..1
    float curve { 0.0f };  // -1..1, krzywizna segmentu WCHODZĄCEGO do tego punktu
};

// Płaska, bez-alokacyjna migawka obwiedni czytana przez wątek audio.
struct EnvSnapshot
{
    static constexpr int maxPoints = 32;

    int   numPoints    { 0 };
    int   sustainIndex { 0 };
    float times[maxPoints]  {};
    float levels[maxPoints] {};
    float curves[maxPoints] {};

    float totalLength() const noexcept
    {
        return numPoints > 0 ? times[numPoints - 1] : 0.0f;
    }
};

// Kształtowanie segmentu krzywizną c (-1..1; 0 = liniowo). f w [0,1].
inline float fiEnvShape (float f, float c) noexcept
{
    f = juce::jlimit (0.0f, 1.0f, f);
    if (std::abs (c) < 1.0e-4f)
        return f;

    const float k = c * 6.0f;
    return (std::exp (k * f) - 1.0f) / (std::exp (k) - 1.0f);
}

// Ewaluacja poziomu obwiedni w czasie t (sekundy) w zakresie punktów [from..to].
inline float fiEnvEvalRange (const EnvSnapshot& s, int from, int to, float t) noexcept
{
    if (to <= from)            return s.levels[from];
    if (t <= s.times[from])    return s.levels[from];
    if (t >= s.times[to])      return s.levels[to];

    for (int i = from; i < to; ++i)
    {
        if (t <= s.times[i + 1])
        {
            const float t0   = s.times[i];
            const float t1   = s.times[i + 1];
            const float span = juce::jmax (1.0e-6f, t1 - t0);
            const float f    = (t - t0) / span;
            return s.levels[i] + (s.levels[i + 1] - s.levels[i]) * fiEnvShape (f, s.curves[i + 1]);
        }
    }
    return s.levels[to];
}

// Edytowalny model — własność wątku GUI.
class EnvelopeModel
{
public:
    EnvelopeModel() { setDefault(); }

    std::vector<EnvPoint> points;
    int sustainIndex { 2 };

    // Domyślnie odwzorowuje stare ADSR: A=0.05 D=0.1 S=0.8 R=0.4.
    void setDefault()
    {
        points = {
            { 0.00f, 0.0f, 0.0f },
            { 0.05f, 1.0f, 0.0f },
            { 0.15f, 0.8f, 0.0f },
            { 0.55f, 0.0f, 0.0f },
        };
        sustainIndex = 2;
    }

    void sortAndClamp()
    {
        if (points.size() < 2)
        {
            setDefault();
            return;
        }

        std::sort (points.begin(), points.end(),
                   [] (const EnvPoint& a, const EnvPoint& b) { return a.time < b.time; });

        points.front().time = 0.0f;

        for (auto& p : points)
        {
            p.level = juce::jlimit (0.0f, 1.0f, p.level);
            p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
        }

        for (size_t i = 1; i < points.size(); ++i)
            if (points[i].time <= points[i - 1].time)
                points[i].time = points[i - 1].time + 1.0e-3f;

        if (points.size() > (size_t) EnvSnapshot::maxPoints)
            points.resize ((size_t) EnvSnapshot::maxPoints);

        sustainIndex = juce::jlimit (0, (int) points.size() - 1, sustainIndex);
    }

    EnvSnapshot makeSnapshot() const
    {
        EnvSnapshot s;
        s.numPoints    = juce::jmin ((int) points.size(), EnvSnapshot::maxPoints);
        s.sustainIndex = juce::jlimit (0, juce::jmax (0, s.numPoints - 1), sustainIndex);

        for (int i = 0; i < s.numPoints; ++i)
        {
            s.times[i]  = points[(size_t) i].time;
            s.levels[i] = points[(size_t) i].level;
            s.curves[i] = points[(size_t) i].curve;
        }
        return s;
    }

    // --- Serializacja do ValueTree (presety / stan w DAW) ---
    void toValueTree (juce::ValueTree& tree) const
    {
        tree.removeAllChildren (nullptr);
        tree.setProperty ("sustainIndex", sustainIndex, nullptr);

        for (const auto& p : points)
        {
            juce::ValueTree pt ("PT");
            pt.setProperty ("t", p.time,  nullptr);
            pt.setProperty ("l", p.level, nullptr);
            pt.setProperty ("c", p.curve, nullptr);
            tree.appendChild (pt, nullptr);
        }
    }

    void fromValueTree (const juce::ValueTree& tree)
    {
        if (! tree.isValid() || tree.getNumChildren() == 0)
            return;

        points.clear();
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto pt = tree.getChild (i);
            points.push_back ({ (float) pt.getProperty ("t"),
                                (float) pt.getProperty ("l"),
                                (float) pt.getProperty ("c") });
        }
        sustainIndex = (int) tree.getProperty ("sustainIndex", 2);
        sortAndClamp();
    }
};
