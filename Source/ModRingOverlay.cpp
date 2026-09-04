#include "ModRingOverlay.h"
#include "EnvelopeEditor.h"
#include "SpiralVisualizer.h"
#include "SynthVoice.h"
#include "ModMath.h"

namespace
{
    // Wysokość textboxa pod gałkami rotary (setTextBoxStyle w PluginEditor).
    constexpr float rotaryTextBoxH = 16.0f;

    // Łuk zakresu modulacji wokół gałki rotary + kropki głosów na żywo.
    // modded(base, a) liczy wartość parametru po modulacji a = amount·env.
    template <typename Fn>
    void drawRotaryRing (juce::Graphics& g, juce::Slider* s, juce::Colour col,
                         float amt, const float* envVal, int numVoices, Fn&& modded)
    {
        if (s == nullptr)
            return;

        const auto  knob   = s->getBounds().toFloat().withTrimmedBottom (rotaryTextBoxH);
        const float radius = juce::jmin (knob.getWidth(), knob.getHeight()) * 0.5f - 3.0f;
        const auto  centre = knob.getCentre();
        const auto  rp     = s->getRotaryParameters();

        auto angleOf = [&] (double value)
        {
            const auto prop = (float) s->valueToProportionOfLength (value);
            return rp.startAngleRadians
                 + juce::jlimit (0.0f, 1.0f, prop) * (rp.endAngleRadians - rp.startAngleRadians);
        };

        const float base = (float) s->getValue();
        const float a0   = angleOf (base);
        const float a1   = angleOf (modded (base, amt));   // pełne wychylenie (env = 1)

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, a0, a1, true);
        g.setColour (col.withAlpha (0.75f));
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        g.setColour (col);
        for (int i = 0; i < numVoices; ++i)
        {
            const float av  = angleOf (modded (base, amt * envVal[i]));
            const auto  pos = centre.getPointOnCircumference (radius, av);
            g.fillEllipse (pos.x - 3.0f, pos.y - 3.0f, 6.0f, 6.0f);
        }
    }

    // Wariant dla suwaka poziomego: kreska zakresu pod torem + kropki głosów.
    template <typename Fn>
    void drawLinearRing (juce::Graphics& g, juce::Slider* s, juce::Colour col,
                         float amt, const float* envVal, int numVoices, Fn&& modded)
    {
        if (s == nullptr)
            return;

        const float base = (float) s->getValue();
        auto xOf = [&] (double value)
        {
            return (float) s->getX() + s->getPositionOfValue (value);
        };

        const float y = (float) s->getBottom() - 2.0f;
        g.setColour (col.withAlpha (0.75f));
        g.drawLine (xOf (base), y, xOf (modded (base, amt)), y, 3.0f);

        g.setColour (col);
        for (int i = 0; i < numVoices; ++i)
        {
            const float xv = xOf (modded (base, amt * envVal[i]));
            g.fillEllipse (xv - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
    }

    // Wzory modulacji per cel — ze wspólnego ModMath.h (te same, którymi
    // renderNextBlock liczy dźwięk; patrz tam uwaga o LFO poza ringiem).
    // modCutoff potrzebuje sample rate (sufit = Nyquist), więc powstaje jako
    // lambda w paint() — reszta celów nie zależy od sr i zostaje wolnymi funkcjami.
    float modResonance (float base, float a) { return fiMod::clampResonance (base * fiMod::gainFactor (a)); }
    float modMix       (float base, float a) { return juce::jlimit (0.0f, 1.0f, base * fiMod::gainFactor (a)); }
    float modStretch   (float base, float a) { return fiMod::stretchTarget (base, a); }
    float modFm        (float base, float a) { return fiMod::fmTarget (base, a); }
    float modTilt      (float base, float a) { return fiMod::tiltTarget (base, a); }
}

ModRingOverlay::ModRingOverlay (FiSynthAudioProcessor& p, const Targets& t)
    : processor (p), targets (t)
{
    setInterceptsMouseClicks (false, false);

    for (int slot = 0; slot < 3; ++slot)
    {
        const juce::String prefix = "env" + juce::String (slot + 1);
        destPar[slot] = processor.apvts.getRawParameterValue (prefix + "Dest");
        amtPar[slot]  = processor.apvts.getRawParameterValue (prefix + "Amt");
    }

    for (int o = 0; o < 3; ++o)
    {
        const juce::String prefix = "osc" + juce::String (o + 1);
        stretchRaw[o] = processor.apvts.getRawParameterValue (prefix + "stretch");
        modeRaw[o]    = processor.apvts.getRawParameterValue (prefix + "stretchmode");
    }

    startTimerHz (30);
}

ModRingOverlay::~ModRingOverlay()
{
    stopTimer();
}

int ModRingOverlay::voiceEnvValues (int envIdx, float* out) const
{
    const auto& snap = snaps[envIdx - 1];
    int n = 0;

    for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
    {
        const float t = processor.envPlayheadTime[envIdx][i].load();
        if (t < 0.0f)
            continue;

        out[n++] = (snap.numPoints >= 2)
                 ? fiEnvEvalRange (snap, 0, snap.numPoints - 1, t)
                 : 0.0f;
    }
    return n;
}

void ModRingOverlay::paint (juce::Graphics& g)
{
    // Ten sam sufit cutoffa co w DSP — inaczej ring kłamałby o zakresie ruchu
    // przy sample rate poniżej ~40 kHz (patrz fiMod::clampCutoff).
    const double sr = processor.getSampleRate();
    const auto modCutoff = [sr] (float base, float a)
    {
        return fiMod::clampCutoff (base * fiMod::cutoffFactor (a), sr);
    };

    for (int slot = 0; slot < 3; ++slot)
    {
        const int   dest = (int) destPar[slot]->load();
        const float amt  = amtPar[slot]->load();
        if (dest <= 0 || std::abs (amt) < 1.0e-3f)
            continue;

        const auto colour = EnvelopeEditor::envelopeColour (slot + 1);

        float envVal[FiSynthAudioProcessor::kMaxPlayheads];
        const int numVoices = voiceEnvValues (slot + 1, envVal);

        switch (dest)
        {
            case FiSynthVoice::ModCutoff:
                drawLinearRing (g, targets.cutoff, colour, amt, envVal, numVoices, modCutoff);
                break;

            case FiSynthVoice::ModResonance:
                drawLinearRing (g, targets.resonance, colour, amt, envVal, numVoices, modResonance);
                break;

            case FiSynthVoice::ModOscMix:
                // mixMul skaluje wszystkie oscylatory naraz — pierścień na każdej gałce Mix.
                for (int o = 0; o < 3; ++o)
                    drawRotaryRing (g, targets.mix[o], colour, amt, envVal, numVoices, modMix);
                break;

            case FiSynthVoice::ModStretch1:
            case FiSynthVoice::ModStretch2:
            case FiSynthVoice::ModStretch3:
                // Ring stretcha żyje na spirali: kreska wzdłuż promienia trybu
                // (od bazy do pełnego wychylenia) + kropki głosów na żywo.
                if (targets.spiral != nullptr)
                {
                    const int  o    = dest - FiSynthVoice::ModStretch1;
                    const float base = stretchRaw[o]->load();
                    const auto  off  = targets.spiral->getPosition().toFloat();

                    const auto p0 = targets.spiral->stretchPointFor (o, base) + off;
                    const auto p1 = targets.spiral->stretchPointFor (o, modStretch (base, amt)) + off;
                    g.setColour (colour.withAlpha (0.75f));
                    g.drawLine ({ p0, p1 }, 3.0f);

                    g.setColour (colour);
                    for (int i = 0; i < numVoices; ++i)
                    {
                        const auto pv = targets.spiral->stretchPointFor (
                                            o, modStretch (base, amt * envVal[i])) + off;
                        g.fillEllipse (pv.x - 3.0f, pv.y - 3.0f, 6.0f, 6.0f);
                    }
                }
                break;

            case FiSynthVoice::ModFM:
                drawLinearRing (g, targets.fm, colour, amt, envVal, numVoices, modFm);
                break;

            case FiSynthVoice::ModTilt1:
            case FiSynthVoice::ModTilt2:
            case FiSynthVoice::ModTilt3:
                drawRotaryRing (g, targets.tilt[dest - FiSynthVoice::ModTilt1],
                                colour, amt, envVal, numVoices, modTilt);
                break;

            default:
                break;   // Pitch nie ma kontrolki w GUI
        }
    }
}

void ModRingOverlay::collectDirtyRects (juce::RectangleList<int>& rects) const
{
    auto add = [&rects] (juce::Slider* s)
    {
        if (s != nullptr)
            rects.add (s->getBounds().expanded (10));
    };

    for (int slot = 0; slot < 3; ++slot)
    {
        const int   dest = (int) destPar[slot]->load();
        const float amt  = amtPar[slot]->load();
        if (dest <= 0 || std::abs (amt) < 1.0e-3f)
            continue;

        switch (dest)
        {
            case FiSynthVoice::ModCutoff:    add (targets.cutoff);    break;
            case FiSynthVoice::ModResonance: add (targets.resonance); break;

            case FiSynthVoice::ModOscMix:
                for (int o = 0; o < 3; ++o)
                    add (targets.mix[o]);
                break;

            case FiSynthVoice::ModStretch1:
            case FiSynthVoice::ModStretch2:
            case FiSynthVoice::ModStretch3:
                // Promień ringu może przebiegać przez całą tarczę spirali.
                if (targets.spiral != nullptr)
                    rects.add (targets.spiral->getBounds().expanded (4));
                break;

            case FiSynthVoice::ModFM:
                add (targets.fm);
                break;

            case FiSynthVoice::ModTilt1:
            case FiSynthVoice::ModTilt2:
            case FiSynthVoice::ModTilt3:
                add (targets.tilt[dest - FiSynthVoice::ModTilt1]);
                break;

            default:
                break;
        }
    }
}

void ModRingOverlay::timerCallback()
{
    // Świeże migawki po każdej edycji obwiedni (makeSnapshot jest tani).
    const int ec = processor.envEditCount.load();
    if (ec != lastEditCount)
    {
        lastEditCount = ec;
        for (int e = 0; e < 3; ++e)
            snaps[e] = processor.getEnvelopeModel (e + 1).makeSnapshot();
    }

    // Sygnatura stanu — przerysowujemy tylko przy zmianie i tylko obszary celów.
    float sig[sigLen];
    int k = 0;

    for (int slot = 0; slot < 3; ++slot)
    {
        sig[k++] = destPar[slot]->load();
        sig[k++] = amtPar[slot]->load();
    }
    for (int e = 1; e <= 3; ++e)
        for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
            sig[k++] = processor.envPlayheadTime[e][i].load();

    auto sv = [&] (juce::Slider* s) { sig[k++] = (s != nullptr) ? (float) s->getValue() : 0.0f; };
    sv (targets.cutoff);
    sv (targets.resonance);
    sv (targets.fm);
    for (int o = 0; o < 3; ++o)
    {
        sv (targets.mix[o]);
        sv (targets.tilt[o]);
        sig[k++] = stretchRaw[o]->load();
        sig[k++] = modeRaw[o]->load();
    }
    sig[k++] = (float) ec;
    jassert (k == sigLen);

    bool changed = false;
    for (int i = 0; i < sigLen; ++i)
        if (! juce::exactlyEqual (sig[i], lastSig[i]))
        {
            changed = true;
            break;
        }

    if (! changed)
        return;

    std::memcpy (lastSig, sig, sizeof (sig));

    juce::RectangleList<int> rects;
    collectDirtyRects (rects);

    auto toRepaint = rects;
    toRepaint.add (lastRects);   // wyczyść też miejsca po poprzednich pierścieniach
    for (const auto& r : toRepaint)
        repaint (r);

    lastRects = rects;
}
