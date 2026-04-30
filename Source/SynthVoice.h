#pragma once

#include <JuceHeader.h>
#include "SynthSound.h"

class FiSynthVoice : public juce::SynthesiserVoice
{
public:
    FiSynthVoice()
    {
        adsrParams.attack  = 0.05f;
        adsrParams.decay   = 0.1f;
        adsrParams.sustain = 0.8f;
        adsrParams.release = 0.4f;
        adsr.setParameters (adsrParams);
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<FiSynthSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle    = 0.0;
        level           = velocity * 0.15f;
        frequency       = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        angleDelta      = (frequency / getSampleRate()) * juce::MathConstants<double>::twoPi;
        adsr.noteOn();
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            clearCurrentNote();
            adsr.reset();
        }
    }

    void pitchWheelMoved (int) override     {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (! adsr.isActive())
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Naiwna saw — na start. Później BLEP/PolyBLEP.
            const float phase    = static_cast<float> (currentAngle / juce::MathConstants<double>::twoPi);
            const float sawValue = 2.0f * phase - 1.0f;
            const float env      = adsr.getNextSample();
            const float out      = sawValue * level * env;

            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample (ch, startSample + sample, out);

            currentAngle += angleDelta;
            if (currentAngle >= juce::MathConstants<double>::twoPi)
                currentAngle -= juce::MathConstants<double>::twoPi;
        }

        if (! adsr.isActive())
            clearCurrentNote();
    }

    void setCurrentPlaybackSampleRate (double sampleRate) override
    {
        if (sampleRate > 0.0)
        {
            juce::SynthesiserVoice::setCurrentPlaybackSampleRate (sampleRate);
            adsr.setSampleRate (sampleRate);
        }
    }

private:
    double currentAngle { 0.0 };
    double angleDelta   { 0.0 };
    double frequency    { 0.0 };
    float  level        { 0.0f };

    juce::ADSR              adsr;
    juce::ADSR::Parameters  adsrParams;
};
