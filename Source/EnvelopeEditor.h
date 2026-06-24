#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// === Graficzny edytor wielopunktowej obwiedni ===
//
// Wyświetla WSZYSTKIE obwiednie (kNumEnvelopes) naraz: aktywna rysowana jest
// pełnym kolorem z punktami do edycji, pozostałe wyblakłą cienką linią w tle.
// Edytowana jest tylko obwiednia aktywna (setActiveEnvelope).
//
// - przeciąganie punktów (czas + poziom)
// - dwuklik na pustym: dodaje punkt; dwuklik na punkcie: usuwa
// - Shift + klik na punkcie: ustawia go jako punkt sustain
// - kreska playheada lecąca w czasie rzeczywistym po krzywej (timer 60 Hz)
class EnvelopeEditor : public juce::Component,
                       private juce::Timer
{
public:
    explicit EnvelopeEditor (FiSynthAudioProcessor& p);
    ~EnvelopeEditor() override;

    // Wybór obwiedni edytowanej w danym momencie (0..kNumEnvelopes-1).
    void setActiveEnvelope (int idx);
    int  getActiveEnvelope() const noexcept { return activeEnv; }

    // Kolor przypisany danej obwiedni (spójny z zakładkami w edytorze).
    static juce::Colour envelopeColour (int idx);

    // Kolor przypisany danej główce playheada (po jednym na grający głos).
    static juce::Colour playheadColour (int idx);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    void  updateView();
    juce::Rectangle<float> plotBounds() const;
    float timeToX (float t) const;
    float levelToY (float l) const;
    float xToTime (float x) const;
    float yToLevel (float y) const;
    int   findPointNear (juce::Point<float> pos, float radius) const;

    // Znajduje uchwyt krzywizny (środek segmentu) blisko pozycji; zwraca indeks
    // LEWEGO punktu segmentu (krzywizna trzymana jest w points[seg+1].curve).
    int   findCurveHandleNear (juce::Point<float> pos, float radius) const;

    // Zaokrągla czas do najbliższej linii siatki (tylko gdy sync + snap włączone).
    float snapTimeToGrid (float t) const;

    // Skrót do aktualnie edytowanej obwiedni.
    EnvelopeModel& activeModel() const { return processor.getEnvelopeModel (activeEnv); }

    FiSynthAudioProcessor& processor;

    int   activeEnv     { 0 };     // która obwiednia jest edytowana
    float viewLength    { 1.0f };  // czas (s) na prawej krawędzi wykresu
    int   draggingPoint { -1 };
    int   draggingCurve { -1 };   // segment, którego krzywiznę przeciągamy (indeks lewego punktu)

    // Do kMaxPlayheads kresek (po jednej na grający głos). Time = czas bezwzględny
    // w jednostkach obwiedni (mapowany przez timeToX), -1 = slot pusty. Poziom (Y)
    // liczymy w paint z krzywej w tym czasie, więc kropka zawsze leży na obwiedni.
    float playheadTime[FiSynthAudioProcessor::kMaxPlayheads];
    int   playheadNote[FiSynthAudioProcessor::kMaxPlayheads];

    // Ostatnio widziany stan siatki — by przerysować gdy się zmieni.
    bool  lastSync { false };
    float lastGrid { 0.0f };
    bool  lastSnap { true };

    static constexpr float maxTime = 16.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
};
