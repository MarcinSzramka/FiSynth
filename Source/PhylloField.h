#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MidiLearn.h"

// === Pole stereo phyllotaxis ===
//
// Pokazuje, gdzie w panoramie ląduje każdy partial: pozycja pozioma =
// frac(n/φ)·spread zmapowane na L..R (ten sam wzór co panTab w SynthVoice),
// wysokość = numer partiala (fundament na dole), wielkość = amplituda saw.
// Przy spread=0 wszystko w pionowej linii (mono), odkręcanie "rozkwita" pole
// złotym rozkładem — bez zbitek, jak nasiona słonecznika.
class PhylloField : public juce::Component, private juce::Timer
{
public:
    explicit PhylloField (FiSynthAudioProcessor& p);
    ~PhylloField() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    FiSynthAudioProcessor& processor;

    LearnSlider  spreadSlider;
    juce::Label  spreadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;

    std::atomic<float>* spreadPar { nullptr };
    float lastSpread { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhylloField)
};
