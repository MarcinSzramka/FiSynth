#include "SpiralVisualizer.h"
#include "PartialTables.h"
#include "FiLook.h"
#include "MidiLearn.h"
#include <cstring>

namespace
{
    const double logPhi = std::log (fiPhi);

    // Tryby rozłożone równo od godziny 12 zgodnie ze wskazówkami; luka między
    // ostatnim a pierwszym to "szew" nie-cyklicznego parametru (0..8).
    // Nazwy i liczba trybów: PartialTables.h (jedno źródło z tablicą ratio).
    constexpr float modeStep = juce::MathConstants<float>::twoPi / (float) fiNumStretchModes;
}

SpiralVisualizer::SpiralVisualizer (FiSynthAudioProcessor& p)
    : processor (p)
{
    for (int o = 0; o < 3; ++o)
    {
        const juce::String prefix = "osc" + juce::String (o + 1);
        par[o].waveform    = processor.apvts.getRawParameterValue (prefix + "waveform");
        par[o].detune      = processor.apvts.getRawParameterValue (prefix + "detune");
        par[o].mix         = processor.apvts.getRawParameterValue (prefix + "mix");
        par[o].stretch     = processor.apvts.getRawParameterValue (prefix + "stretch");
        par[o].stretchMode = processor.apvts.getRawParameterValue (prefix + "stretchmode");
        par[o].goldInt     = processor.apvts.getRawParameterValue (prefix + "goldint");

        oscButtons[o].setButtonText (juce::String (o + 1));
        oscButtons[o].setClickingTogglesState (true);
        oscButtons[o].setRadioGroupId (2001);
        oscButtons[o].setColour (juce::TextButton::buttonOnColourId,
                                 fiOscColour (o).withMultipliedBrightness (0.55f));
        oscButtons[o].onClick = [this, o] { selectOsc (o); };
        addAndMakeVisible (oscButtons[o]);
    }

    oscButtons[0].setToggleState (true, juce::dontSendNotification);
    selectOsc (0);

    startTimerHz (30);
}

SpiralVisualizer::~SpiralVisualizer()
{
    stopTimer();
}

void SpiralVisualizer::selectOsc (int o)
{
    curOsc = o;
    const juce::String prefix = "osc" + juce::String (o + 1);
    stretchParam = processor.apvts.getParameter (prefix + "stretch");
    modeParam    = processor.apvts.getParameter (prefix + "stretchmode");

    stretchAtt = std::make_unique<juce::ParameterAttachment> (*stretchParam,
        [this] (float v) { curStretch = v; repaint(); });
    modeAtt = std::make_unique<juce::ParameterAttachment> (*modeParam,
        [this] (float v) { curMode = v; repaint(); });

    stretchAtt->sendInitialUpdate();
    modeAtt->sendInitialUpdate();
}

int SpiralVisualizer::activeNote() const
{
    return processor.firstActiveNote();
}

void SpiralVisualizer::resized()
{
    // Przyciski 1/2/3 w pasku tytułu (prawy górny róg tarczy).
    auto row = getLocalBounds().reduced (8).removeFromTop (18);
    for (int o = 2; o >= 0; --o)
        oscButtons[o].setBounds (row.removeFromRight (26).reduced (1, 0));
}

SpiralVisualizer::Geo SpiralVisualizer::plotGeometry() const
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    area.removeFromTop (20.0f);       // tytuł + przyciski osc
    area.removeFromBottom (16.0f);    // stopka
    area.removeFromBottom (16.0f);    // legenda

    const float side = juce::jmin (area.getWidth(), area.getHeight());
    auto plot = area.withSizeKeepingCentre (side, side);

    // Margines na pierścień etykiet trybów wokół tarczy.
    return { plot.getCentre(), 6.0f, side * 0.5f - 26.0f };
}

float SpiralVisualizer::modeAngle (float mode) noexcept
{
    return juce::jlimit (0.0f, (float) (fiNumStretchModes - 1), mode) * modeStep;
}

juce::Rectangle<float> SpiralVisualizer::modeLabelRect (int m) const
{
    const auto geo = plotGeometry();
    const auto pt  = geo.centre.getPointOnCircumference (geo.rMax + 15.0f,
                                                         modeAngle ((float) m));
    return juce::Rectangle<float> (58.0f, 12.0f).withCentre (pt);
}

void SpiralVisualizer::applyPointer (const juce::MouseEvent& e)
{
    const auto geo = plotGeometry();
    const auto d   = e.position - geo.centre;

    const float r       = d.getDistanceFromOrigin();
    const float stretch = juce::jlimit (0.0f, 1.0f,
                                        (r - geo.r0) / juce::jmax (1.0f, geo.rMax - geo.r0));
    if (stretchAtt)
        stretchAtt->setValueAsPartOfGesture (stretch);

    // Shift = zamek trybu: korygujesz sam stretch bez naruszania czystego
    // trybu ustawionego np. klikiem w etykietę (dawna gwarancja osobnej gałki).
    if (e.mods.isShiftDown())
        return;

    // Kąt od godziny 12, zgodnie ze wskazówkami (konwencja tarczy).
    float ang = std::atan2 (d.x, -d.y);
    if (ang < 0.0f)
        ang += juce::MathConstants<float>::twoPi;

    // Luka za ostatnim trybem: przyciągnij do bliższego skraju (ostatni albo 0).
    constexpr float lastMode = (float) (fiNumStretchModes - 1);
    float mode = ang / modeStep;
    if (mode > lastMode)
        mode = (mode > lastMode + 0.5f) ? 0.0f : lastMode;

    if (modeAtt)
        modeAtt->setValueAsPartOfGesture (mode);
}

void SpiralVisualizer::mouseDown (const juce::MouseEvent& e)
{
    // Prawy klik = menu MIDI Learn obu parametrów tarczy (promień = stretch,
    // kąt = tryb) dla AKTYWNEGO oscylatora — zanim cokolwiek zacznie gest.
    if (e.mods.isPopupMenu())
    {
        const juce::String prefix = "osc" + juce::String (curOsc + 1);
        juce::PopupMenu menu;
        addMidiLearnItems (menu, processor, prefix + "stretch");
        addMidiLearnItems (menu, processor, prefix + "stretchmode");
        menu.showMenuAsync (juce::PopupMenu::Options());
        return;
    }

    // Klik w etykietę trybu = skok do wartości całkowitej (dawny ComboBox).
    for (int m = 0; m < fiNumStretchModes; ++m)
    {
        if (modeLabelRect (m).expanded (2.0f).contains (e.position))
        {
            if (modeAtt)
            {
                modeAtt->beginGesture();
                modeAtt->setValueAsPartOfGesture ((float) m);
                modeAtt->endGesture();
            }
            return;
        }
    }

    const auto geo = plotGeometry();
    if (e.position.getDistanceFrom (geo.centre) > geo.rMax + 10.0f)
        return;

    dragging = true;
    if (stretchAtt) stretchAtt->beginGesture();
    if (modeAtt)    modeAtt->beginGesture();
    applyPointer (e);
}

void SpiralVisualizer::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        applyPointer (e);
}

void SpiralVisualizer::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    if (stretchAtt) stretchAtt->endGesture();
    if (modeAtt)    modeAtt->endGesture();
}

juce::Point<float> SpiralVisualizer::stretchPointFor (int osc, float stretch) const
{
    const auto geo = plotGeometry();
    const int  o   = juce::jlimit (0, 2, osc);
    const float r  = geo.r0 + (geo.rMax - geo.r0) * juce::jlimit (0.0f, 1.0f, stretch);
    return geo.centre.getPointOnCircumference (r, modeAngle (par[o].stretchMode->load()));
}

void SpiralVisualizer::timerCallback()
{
    float sig[sigLen];
    int k = 0;

    for (int o = 0; o < 3; ++o)
    {
        sig[k++] = par[o].waveform->load();
        sig[k++] = par[o].detune->load();
        sig[k++] = par[o].mix->load();
        sig[k++] = par[o].stretch->load();
        sig[k++] = par[o].stretchMode->load();
        sig[k++] = par[o].goldInt->load();
    }
    sig[k++] = (float) activeNote();
    sig[k++] = (float) processor.getSampleRate();
    jassert (k == sigLen);

    for (int i = 0; i < sigLen; ++i)
    {
        if (! juce::exactlyEqual (sig[i], lastSig[i]))
        {
            std::memcpy (lastSig, sig, sizeof (sig));
            repaint();
            return;
        }
    }
}

void SpiralVisualizer::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (fiCol::widget);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (juce::String (juce::CharPointer_UTF8 (
                    "\xcf\x86-SPIRALA \xc2\xb7 STRETCH \xc3\x97 TRYB")),
                area.removeFromTop (20.0f).withTrimmedLeft (6.0f),
                juce::Justification::centredLeft);

    auto footer = area.removeFromBottom (16.0f);
    auto legend = area.removeFromBottom (16.0f);

    const auto geo    = plotGeometry();
    const auto centre = geo.centre;
    const float rMax  = geo.rMax;

    const int    note = activeNote();
    const double f0   = juce::MidiMessage::getMidiNoteInHertz (note);
    double sr = processor.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;
    const double nyq = sr * 0.5;

    // Ile obrotów spirali mieści się do Nyquista od fundamentu.
    const double maxTurns = juce::jmax (2.0, std::log (nyq / f0) / logPhi);

    auto radiusOf = [&] (double turns)
    {
        return geo.r0 + (rMax - geo.r0) * (float) (turns / maxTurns);
    };
    auto polar = [&] (double turns)
    {
        // Kąt zgodnie z ruchem wskazówek od godziny 12 — obroty·2π.
        return centre.getPointOnCircumference (
            radiusOf (turns), (float) (turns * juce::MathConstants<double>::twoPi));
    };

    // Pierścień kontrolny: obwód tarczy + etykiety trybów (kąt = tryb).
    {
        g.setColour (juce::Colour (0x22e0b64b));
        g.drawEllipse (centre.x - rMax, centre.y - rMax, rMax * 2.0f, rMax * 2.0f, 1.0f);

        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        const int curIdx = (int) std::lround (curMode);
        for (int m = 0; m < fiNumStretchModes; ++m)
        {
            // Kreska-zaczep od obwodu do etykiety.
            const auto pIn  = centre.getPointOnCircumference (rMax,        modeAngle ((float) m));
            const auto pOut = centre.getPointOnCircumference (rMax + 5.0f, modeAngle ((float) m));
            g.setColour (juce::Colour (0x33e0b64b));
            g.drawLine ({ pIn, pOut }, 1.0f);

            g.setColour (m == curIdx ? fiCol::gold : juce::Colour (0x77ffffff));
            g.drawText (fiStretchModeNames[m], modeLabelRect (m), juce::Justification::centred);
        }
    }

    // Prowadnica spirali.
    juce::Path guide;
    guide.startNewSubPath (polar (0.0));
    for (double t = 0.05; t <= maxTurns; t += 0.05)
        guide.lineTo (polar (t));
    g.setColour (juce::Colour (0x26ffffff));
    g.strokePath (guide, juce::PathStrokeType (1.0f));

    // Promień ×φⁿ: tu lądują kolejne potęgi φ (pełne obroty).
    {
        g.setColour (juce::Colour (0x55e0b64b));
        const float dashes[] = { 3.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (centre.x, centre.y, centre.x, centre.y - rMax),
                          dashes, 2, 1.0f);
    }

    // Duchy: pozycje szeregu harmonicznego (stretch = 0) jako odniesienie.
    g.setColour (juce::Colour (0x48ffffff));
    for (int n = 0; n < fiNumPartials; ++n)
    {
        const double turns = std::log ((double) (n + 1)) / logPhi;
        if (turns > maxTurns)
            break;
        const auto pos = polar (turns);
        g.fillEllipse (pos.x - 2.0f, pos.y - 2.0f, 4.0f, 4.0f);
    }

    // Partiale oscylatorów.
    for (int o = 0; o < 3; ++o)
    {
        const int   wf  = (int) par[o].waveform->load();
        const float mix = par[o].mix->load();
        if (wf == 5 || mix <= 0.001f)   // Noise nie ma partiali; mix 0 nie gra
            continue;

        const float cents  = par[o].detune->load()
                           + fiGoldIntCents ((int) par[o].goldInt->load());
        const double fBase = f0 * std::pow (2.0, (double) cents / 1200.0);

        // Ratio ze wspólnego fiStretchedRatios — spirala pokazuje morph 1:1
        // z tym, co silnik wkłada w przyrosty faz.
        double ratios[fiNumPartials];
        fiStretchedRatios (par[o].stretchMode->load(), par[o].stretch->load(), ratios);

        const auto colour = fiOscColour (o).withAlpha (0.55f + 0.45f * juce::jmin (1.0f, mix));

        for (int n = 0; n < fiNumPartials; ++n)
        {
            const float amp = fiHarmonicAmplitude (wf, n);
            if (amp <= 0.0f)   // np. parzyste harmoniczne square — nie grają
                continue;

            const double freq = fBase * ratios[n];
            if (freq >= nyq)   // ponad Nyquistem — silnik go pomija, my też
                continue;

            const double turns = std::log (freq / f0) / logPhi;
            if (radiusOf (turns) < 2.0f)   // goldint −φ/−2φ może zejść pod fundament
                continue;

            const float size = 2.0f + 7.0f * std::pow (amp, 0.4f);
            const auto  pos  = polar (turns);
            g.setColour (colour);
            g.fillEllipse (pos.x - size * 0.5f, pos.y - size * 0.5f, size, size);
        }
    }

    // Znaczniki kontrolera: duchy pozostałych oscylatorów + aktywny z celownikiem.
    for (int o = 0; o < 3; ++o)
    {
        if (o == curOsc)
            continue;
        const auto pt = stretchPointFor (o, par[o].stretch->load());
        g.setColour (fiOscColour (o).withAlpha (0.4f));
        g.drawEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f, 1.4f);
    }
    {
        const float ang = modeAngle (curMode);
        const auto  rim = centre.getPointOnCircumference (rMax, ang);
        const auto  pt  = stretchPointFor (curOsc, curStretch);
        const auto  col = fiOscColour (curOsc);

        // Celownik: promień trybu + okrąg stretcha.
        g.setColour (col.withAlpha (0.30f));
        g.drawLine ({ centre, rim }, 1.2f);
        const float rr = pt.getDistanceFrom (centre);
        if (rr > 3.0f)
            g.drawEllipse (centre.x - rr, centre.y - rr, rr * 2.0f, rr * 2.0f, 0.8f);

        g.setColour (col);
        g.fillEllipse (pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f);
        g.setColour (col.withAlpha (0.5f));
        g.drawEllipse (pt.x - 9.0f, pt.y - 9.0f, 18.0f, 18.0f, 1.4f);
    }

    // Legenda: oscylatory + szare duchy (nieruchoma linijka szeregu harmonicznego).
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    {
        const float itemW = 56.0f, ghostW = 96.0f;
        float x = legend.getCentreX() - (itemW * 3.0f + ghostW) * 0.5f;
        for (int o = 0; o < 3; ++o)
        {
            g.setColour (fiOscColour (o));
            g.fillEllipse (x, legend.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (juce::Colour (0x88ffffff));
            g.drawText ("Osc " + juce::String (o + 1),
                        juce::Rectangle<float> (x + 9.0f, legend.getY(), itemW - 9.0f, legend.getHeight()),
                        juce::Justification::centredLeft);
            x += itemW;
        }
        g.setColour (juce::Colour (0x48ffffff));
        g.fillEllipse (x, legend.getCentreY() - 2.0f, 4.0f, 4.0f);
        g.setColour (juce::Colour (0x66ffffff));
        g.drawText ("szereg harm.",
                    juce::Rectangle<float> (x + 8.0f, legend.getY(), ghostW - 8.0f, legend.getHeight()),
                    juce::Justification::centredLeft);
    }

    // Stopka: nuta + hint Shift + status kontrolera aktywnego oscylatora.
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                    + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  1 obr\xc3\xb3t = \xc3\x97\xcf\x86")),
                footer.withTrimmedLeft (8.0f), juce::Justification::centredLeft);
    g.setColour (juce::Colour (0x55ffffff));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x87\xa7 = tylko stretch")),
                footer, juce::Justification::centred);
    g.setColour (juce::Colour (0x88ffffff));
    g.drawText ("stretch " + juce::String (curStretch, 2)
                    + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 tryb "))
                    + juce::String (curMode, 2),
                footer.withTrimmedRight (8.0f), juce::Justification::centredRight);
}
