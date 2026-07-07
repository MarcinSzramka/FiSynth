#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// === Analizator widma z siatką φⁿ ===
//
// FFT (2048, okno Hanna) na mono sumie wyjścia, dostarczanej lock-free FIFO
// z processBlock. Twist: pionowe linie siatki co φⁿ od granej nuty (przełączane
// na oktawy) — na siatce złotej partiale stretchowanych trybów siadają na
// liniach, na oktawowej wyglądają losowo. Wygładzanie: szybki atak, opadanie
// ~1.2 dB/klatkę (peak-decay), żeby obraz nie migotał.
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (FiSynthAudioProcessor& p);
    ~SpectrumAnalyzer() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int  activeNote() const;

    FiSynthAudioProcessor& processor;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;   // 2048

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    // Pierścień ostatnich próbek + bufor roboczy FFT + wygładzone dB per bin.
    std::vector<float> ring     = std::vector<float> (fftSize, 0.0f);
    std::vector<float> fftData  = std::vector<float> (fftSize * 2, 0.0f);
    std::vector<float> smoothDb = std::vector<float> ((size_t) fftSize / 2, -100.0f);
    int ringPos { 0 };

    juce::TextButton phiGridButton, octGridButton;
    bool phiGrid { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
