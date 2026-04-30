#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthSound.h"
#include "SynthVoice.h"

FiSynthAudioProcessor::FiSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new FiSynthVoice());

    synth.addSound (new FiSynthSound());
}

void FiSynthAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
}

void FiSynthAudioProcessor::releaseResources() {}

bool FiSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void FiSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* FiSynthAudioProcessor::createEditor()
{
    return new FiSynthAudioProcessorEditor (*this);
}

void FiSynthAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void FiSynthAudioProcessor::setStateInformation (const void*, int)  {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FiSynthAudioProcessor();
}
