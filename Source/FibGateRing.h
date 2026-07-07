#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// === Pierścień Fibonacciego — sekwencer/gate ===
//
// Kroki słowa Fibonacciego rozłożone na okręgu: pełna kropka = krok otwarty
// (S), pusta = zamknięty (L). Klik w kropkę flipuje krok (maska bitowa
// gateFlips w procesorze — atomik, więc bez commitów). Wskazówka wiruje po
// bieżącym kroku (gateStep pisany przez audio, sync do BPM/ppq). Kontrolki:
// włącznik, głębokość, podział kroku, długość patternu (generacja słowa).
class FibGateRing : public juce::Component, private juce::Timer
{
public:
    explicit FibGateRing (FiSynthAudioProcessor& p);
    ~FibGateRing() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> ringBounds() const;
    int stepNear (juce::Point<float> pos) const;

    FiSynthAudioProcessor& processor;

    juce::ToggleButton onButton { "Gate" };
    juce::Slider       depthSlider;
    juce::ComboBox     divBox, genBox;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   onAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttachment, genAttachment;

    std::atomic<float>* genPar { nullptr };
    std::atomic<float>* onPar  { nullptr };

    int           lastStep  { -2 };
    juce::uint64  lastFlips { 0 };
    int           lastGen   { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FibGateRing)
};
