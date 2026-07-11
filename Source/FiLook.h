#pragma once

#include <JuceHeader.h>

// === Wspólna paleta GUI ===
// Jedno miejsce na branding: kolory oscylatorów (spirala, legendy, przyciski),
// ciepła "złota" rodzina teł sekcji i LookAndFeel kontrolek. Filozofia:
// niska saturacja brązów/bursztynów jako tło, czyste złoto TYLKO jako akcent φ.

// Kolory oscylatorów — rodzina "antycznego brązu": złoto / patyna / terakota.
// Rozróżnialne na spirali, a nie gryzą się z ciepłym tłem.
inline juce::Colour fiOscColour (int o)
{
    switch (o)
    {
        case 0:  return juce::Colour (0xffe0b64b);  // złoto
        case 1:  return juce::Colour (0xff9ec4a0);  // patyna (zieleń brązu)
        default: return juce::Colour (0xffd98c5f);  // terakota
    }
}

namespace fiCol
{
    inline const juce::Colour bg        { 0xff0b0906 };  // tło okna (ciepła czerń)
    inline const juce::Colour gold      { 0xffe0b64b };  // akcent φ
    inline const juce::Colour goldDim   { 0xff8a6b2a };
    inline const juce::Colour border    { 0x26e0b64b };  // ramki sekcji (złoto ~15%)
    inline const juce::Colour text      { 0xffe8e2d4 };  // ciepła biel
    inline const juce::Colour textDim   { 0x99e8e2d4 };

    // Tła sekcji: ta sama rodzina ciepłych brązów, różnicowane odcieniem —
    // rozróżnialne, ale żadna nie krzyczy.
    inline const juce::Colour osc       { 0xff241e13 };  // brąz
    inline const juce::Colour env       { 0xff1c1f13 };  // oliwkowe złoto
    inline const juce::Colour filter    { 0xff262013 };  // bursztyn
    inline const juce::Colour lfo       { 0xff261a12 };  // siena
    inline const juce::Colour goldSec   { 0xff2e2410 };  // sekcja φ — najjaśniejsza
    inline const juce::Colour gate      { 0xff271d10 };  // rdza wygaszona
    inline const juce::Colour fx        { 0xff2b1a12 };  // przepalona umbra (efektor)
    inline const juce::Colour stereo    { 0xff1e2015 };  // szałwia
    inline const juce::Colour viz       { 0xff17130c };  // studnie wizualizerów
    inline const juce::Colour widget    { 0xff100d08 };  // wnętrza widżetów (spirala itp.)
}

// === Logo "złote kafle" ===
// Podział złotego prostokąta 13×8 na kwadraty 8 5 3 2 1 1 (ciąg Fibonacciego)
// + spirala z ćwierćłuków przez kafle. Rysowane proceduralnie — skaluje się
// ostro do dowolnego rozmiaru i zawsze zgadza się z Assets/fibo-logo.svg
// (ta sama geometria). Wpasowuje znak w 'area' zachowując proporcje 13:8.
inline void fiDrawLogo (juce::Graphics& g, juce::Rectangle<float> area,
                        juce::Colour tile = fiCol::gold,
                        juce::Colour arc  = juce::Colour (0xfff0d488))
{
    const float sc = juce::jmin (area.getWidth() / 13.0f, area.getHeight() / 8.0f);
    const float ox = area.getCentreX() - 6.5f * sc;
    const float oy = area.getCentreY() - 4.0f * sc;

    struct Sq { float x, y, a; };
    constexpr Sq sq[6] = { { 0, 0, 8 }, { 8, 0, 5 }, { 10, 5, 3 },
                           { 8, 6, 2 }, { 8, 5, 1 }, { 9, 5, 1 } };
    for (int i = 0; i < 6; ++i)
    {
        const float pad = juce::jmax (0.35f, sc * 0.10f);
        g.setColour (tile.withAlpha (0.94f - 0.11f * (float) i));
        g.fillRoundedRectangle (ox + sq[i].x * sc + pad, oy + sq[i].y * sc + pad,
                                sq[i].a * sc - 2.0f * pad, sq[i].a * sc - 2.0f * pad,
                                juce::jmax (1.0f, sc * 0.45f));
    }

    // Ćwierćłuki spirali: (środek, promień, kąt od..do) w jednostkach kafli.
    struct Arc { float cx, cy, r, a0, a1; };
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr Arc arcs[5] = {
        {  8, 8, 8, pi,        1.5f * pi },
        {  8, 5, 5, 1.5f * pi, 2.0f * pi },
        { 10, 5, 3, 0.0f,      0.5f * pi },
        { 10, 6, 2, 0.5f * pi, pi        },
        {  9, 6, 1, pi,        1.5f * pi } };

    juce::Path p;
    bool first = true;
    for (const auto& a : arcs)
        for (int k = 0; k <= 14; ++k)
        {
            const float t = a.a0 + (a.a1 - a.a0) * (float) k / 14.0f;
            const float x = ox + (a.cx + a.r * std::cos (t)) * sc;
            const float y = oy + (a.cy + a.r * std::sin (t)) * sc;
            if (first) { p.startNewSubPath (x, y); first = false; }
            else         p.lineTo (x, y);
        }

    g.setColour (arc);
    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.0f, sc * 0.34f),
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

// LookAndFeel całego edytora: gałki/suwaki/comba w złocie na ciepłej czerni.
// Ustawiany raz na płótnie edytora — dzieci dziedziczą.
class FiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FiLookAndFeel()
    {
        auto scheme = getDarkColourScheme();
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::widgetBackground, juce::Colour (0xff1b1610));
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::menuBackground,   juce::Colour (0xff17130c));
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultText,      fiCol::text);
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::defaultFill,      fiCol::goldDim);
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::highlightedFill,  fiCol::gold);
        scheme.setUIColour (juce::LookAndFeel_V4::ColourScheme::outline,          juce::Colour (0xff3a3226));
        setColourScheme (scheme);

        setColour (juce::Slider::thumbColourId,               fiCol::gold);
        setColour (juce::Slider::trackColourId,               fiCol::goldDim);
        setColour (juce::Slider::backgroundColourId,          juce::Colour (0xff2a241a));
        setColour (juce::Slider::rotarySliderFillColourId,    fiCol::gold);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3226));
        setColour (juce::Slider::textBoxTextColourId,         fiCol::text);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0x00000000));

        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1b1610));
        setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff3a3226));
        setColour (juce::ComboBox::arrowColourId,      fiCol::goldDim);
        setColour (juce::ComboBox::textColourId,       fiCol::text);

        setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1b1610));
        setColour (juce::TextButton::buttonOnColourId, fiCol::goldDim);
        setColour (juce::TextButton::textColourOffId,  fiCol::text);
        setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff0b0906));

        setColour (juce::ToggleButton::textColourId, fiCol::text);
        setColour (juce::ToggleButton::tickColourId, fiCol::gold);
        setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff4a4232));

        setColour (juce::Label::textColourId, fiCol::text);

        setColour (juce::PopupMenu::backgroundColourId,          juce::Colour (0xff17130c));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, fiCol::goldDim);
        setColour (juce::PopupMenu::textColourId,                fiCol::text);

        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff14100a));
        setColour (juce::TextEditor::textColourId,       fiCol::text);
        setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xff3a3226));
    }
};
