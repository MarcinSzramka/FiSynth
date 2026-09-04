#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// === Spirala φ — wizualizer partiali I kontroler stretcha ===
//
// WIZUALIZACJA (jak dotąd): spirala logarytmiczna, pełny obrót = ×φ
// częstotliwości (kąt = obroty·2π, obroty = log_φ(f/f0)). Partial = kropka:
// kolor per oscylator, wielkość = amplituda. Pozycje liczone z tej samej
// tablicy fiRatioRow, z której silnik liczy deltaU — obraz z definicji
// odpowiada brzmieniu. Partiale ponad Nyquistem znikają, jak w renderze.
//
// KONTROLER (wchłonięty morph-pad): przeciąganie po tarczy steruje parametrami
// wybranego oscylatora (przyciski 1/2/3):
//   • promień  → stretch (0 w centrum .. 1 na obwodzie),
//   • kąt      → stretchMode (ciągłe 0..8; 9 etykiet trybów wokół tarczy,
//                40° luki między trybem 8 a 0 = wizualny "szew" nie-cyklicznego
//                parametru).
// Klik w etykietę trybu = skok do wartości całkowitej (dawny ComboBox).
// Znaczniki: pełne kółko = aktywny oscylator, obwódki = duchy pozostałych.
//
// stretchPointFor() wystawia geometrię znacznika dla ModRingOverlay — pierścień
// modulacji stretcha rysuje się jako promień na tarczy (funkcja przeniesiona
// 1:1 z dawnych gałek stretch).
//
// Fundament f0 = pierwsza aktualnie grająca nuta (envPlayheadNote), bez grania A2.
class SpiralVisualizer : public juce::Component, private juce::Timer
{
public:
    explicit SpiralVisualizer (FiSynthAudioProcessor& p);
    ~SpiralVisualizer() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // Punkt (współrzędne lokalne) znacznika stretcha oscylatora o dla wartości
    // stretch s — kąt bierze z BIEŻĄCEGO trybu tego oscylatora. Dla ModRingOverlay.
    juce::Point<float> stretchPointFor (int osc, float stretch) const;

private:
    void timerCallback() override;
    int  activeNote() const;
    void selectOsc (int o);
    void applyPointer (const juce::MouseEvent& e);

    // Geometria tarczy (wspólna dla paint / hit-test / overlay).
    struct Geo { juce::Point<float> centre; float r0, rMax; };
    Geo plotGeometry() const;

    static float modeAngle (float mode) noexcept;          // tryb 0..8 -> kąt od 12:00
    juce::Rectangle<float> modeLabelRect (int m) const;     // etykieta trybu przy tarczy

    FiSynthAudioProcessor& processor;

    struct OscParams
    {
        std::atomic<float>* waveform;
        std::atomic<float>* detune;
        std::atomic<float>* mix;
        std::atomic<float>* stretch;
        std::atomic<float>* stretchMode;
        std::atomic<float>* goldInt;
    } par[3];

    // Kontroler: wybór oscylatora + attachmenty parametrów (wzorzec z morph-pada).
    juce::TextButton oscButtons[3];
    juce::RangedAudioParameter* stretchParam { nullptr };
    juce::RangedAudioParameter* modeParam    { nullptr };
    std::unique_ptr<juce::ParameterAttachment> stretchAtt, modeAtt;
    int   curOsc { 0 };
    float curStretch { 0.0f }, curMode { 0.0f };
    bool  dragging { false };

    // Sygnatura parametrów z poprzedniego ticku — repaint tylko przy zmianie.
    static constexpr int sigLen = 3 * 6 + 2;
    float lastSig[sigLen] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpiralVisualizer)
};
