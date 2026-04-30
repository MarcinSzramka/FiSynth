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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessorEditor)
};
