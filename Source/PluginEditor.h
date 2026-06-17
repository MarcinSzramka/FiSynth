#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class FiSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FiSynthAudioProcessorEditor (FiSynthAudioProcessor&);
    ~FiSynthAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FiSynthAudioProcessor& processorRef;

    juce::Slider gainSlider, stretchSlider;
    juce::Label gainLabel, stretchLabel;

    // ADSR
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;

    // 3 Oscylatory
    struct OscControl
    {
        juce::ComboBox waveformBox;
        juce::Slider detuneSlider;
        juce::Slider mixSlider;
        juce::Label waveformLabel, detuneLabel, mixLabel;

        using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::unique_ptr<ComboBoxAttachment> waveformAttachment;
        std::unique_ptr<SliderAttachment> detuneAttachment;
        std::unique_ptr<SliderAttachment> mixAttachment;
    } oscs[3];

    // Filter
    juce::Slider filterCutoffSlider, filterResonanceSlider;
    juce::ComboBox filterTypeBox;
    juce::Label filterCutoffLabel, filterResonanceLabel, filterTypeLabel;

    // LFO
    juce::Slider lfoRateSlider, lfoDepthSlider;
    juce::ComboBox lfoShapeBox;
    juce::Label lfoRateLabel, lfoDepthLabel, lfoShapeLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> stretchAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> sustainAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterResonanceAttachment;
    std::unique_ptr<ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<ComboBoxAttachment> lfoShapeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessorEditor)
};
