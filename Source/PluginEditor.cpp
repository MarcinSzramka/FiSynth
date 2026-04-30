#include "PluginEditor.h"

FiSynthAudioProcessorEditor::FiSynthAudioProcessorEditor (FiSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (480, 320);
}

void FiSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));

    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::Font (juce::FontOptions (28.0f).withStyle ("Bold")));
    g.drawFittedText ("FiSynth", getLocalBounds().removeFromTop (80),
                      juce::Justification::centred, 1);

    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.setColour (juce::Colours::grey);
    g.drawFittedText ("subtractive synth — v0.1",
                      getLocalBounds().removeFromTop (120),
                      juce::Justification::centredBottom, 1);
}

void FiSynthAudioProcessorEditor::resized() {}
