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

    void setOscillatorParams (int oscIndex, int waveform, float detuneAmount, float mix)
    {
        if (oscIndex >= 0 && oscIndex < 3)
        {
            oscs[oscIndex].waveform = juce::jlimit (0, 5, waveform);
            oscs[oscIndex].detune = detuneAmount;
            oscs[oscIndex].mix = juce::jlimit (0.0f, 1.0f, mix);
        }
    }

    void setStretchAmount (float stretch)
    {
        stretchAmount = juce::jlimit (0.0f, 1.0f, stretch);
    }

    void setADSR (float a, float d, float s, float r)
    {
        adsrParams.attack = a;
        adsrParams.decay = d;
        adsrParams.sustain = s;
        adsrParams.release = r;
        adsr.setParameters (adsrParams);
    }

    void setFilterParams (float cutoff, float resonance, int type)
    {
        filterCutoff = juce::jlimit (20.0f, 20000.0f, cutoff);
        filterResonance = juce::jlimit (0.1f, 10.0f, resonance);
        filterType = juce::jlimit (0, 2, type);
    }

    void setLFOParams (float rate, float depth, int shape)
    {
        lfoRate = juce::jlimit (0.1f, 20.0f, rate);
        lfoDepth = juce::jlimit (0.0f, 1.0f, depth);
        lfoShape = juce::jlimit (0, 2, shape);
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        level = velocity * 0.15f;
        frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

        constexpr float phi = 1.618034f;

        for (int o = 0; o < 3; ++o)
        {
            float detuneHz = frequency * (std::pow (2.0f, oscs[o].detune / 1200.0f) - 1.0f);
            float oscFreq = frequency + detuneHz;

            for (int n = 0; n < numPartials; ++n)
            {
                oscs[o].currentPhase[n] = 0.0;

                float freqHarmonic = oscFreq * (n + 1.0f);
                float freqPhi = oscFreq * std::pow (phi, (float)n);
                float freqStretched = freqHarmonic + (freqPhi - freqHarmonic) * stretchAmount;

                oscs[o].angleDelta[n] = (freqStretched / getSampleRate()) * juce::MathConstants<double>::twoPi;
            }
        }

        lfoPhase = 0.0;
        lfoAngleDelta = (lfoRate / getSampleRate()) * juce::MathConstants<double>::twoPi;

        adsr.noteOn();
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            clearCurrentNote();
            adsr.reset();
        }
    }

    void pitchWheelMoved (int) override     {}
    void controllerMoved (int, int) override {}

    float getHarmonicAmplitude (int waveform, int n)
    {
        float baseAmp = 1.0f / std::sqrt (1.0f + n);

        switch (waveform)
        {
            case 0:  // Sine
                return (n == 0) ? 1.0f : 0.0f;

            case 1:  // Square
                if (n % 2 == 0) return 0.0f;
                return baseAmp / (n + 1.0f);

            case 2:  // Triangle
                if (n % 2 == 0) return 0.0f;
                return baseAmp / ((n + 1.0f) * (n + 1.0f));

            case 3:  // Sawtooth
                return baseAmp / (n + 1.0f);

            case 4:  // Quadratic
                if (n % 2 == 1) return 0.0f;
                return baseAmp / (n + 1.0f);

            case 5:  // Noise
                return baseAmp * 0.1f;

            default:
                return baseAmp;
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override
    {
        if (! adsr.isActive())
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float lfoValue = getLFOValue();
            float cutoffModulated = filterCutoff * (1.0f + lfoValue * lfoDepth * 0.5f);
            cutoffModulated = juce::jlimit (20.0f, 20000.0f, cutoffModulated);

            filter.setCutoffFrequency (cutoffModulated);
            filter.setResonance (filterResonance);
            switch (filterType)
            {
                case 0: filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass); break;
                case 1: filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
                case 2: filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
            }

            float outMix = 0.0f;
            for (int o = 0; o < 3; ++o)
            {
                if (oscs[o].mix <= 0.0f) continue;

                float out = 0.0f;
                if (oscs[o].waveform == 5)
                    out = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                else
                {
                    for (int n = 0; n < numPartials; ++n)
                    {
                        float amp = getHarmonicAmplitude (oscs[o].waveform, n);
                        if (oscs[o].waveform == 4)
                            out += amp * std::cos (2.0f * (float)oscs[o].currentPhase[n]);
                        else
                            out += amp * std::sin ((float)oscs[o].currentPhase[n]);
                        oscs[o].currentPhase[n] += oscs[o].angleDelta[n];
                    }
                }
                outMix += out * oscs[o].mix;
            }

            float filtered = filter.processSample (0, outMix);

            const float env = adsr.getNextSample();
            const float result = filtered * level * env * 0.3f;

            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                outputBuffer.addSample (ch, startSample + sample, result);

            lfoPhase += lfoAngleDelta;
            if (lfoPhase >= juce::MathConstants<double>::twoPi)
                lfoPhase -= juce::MathConstants<double>::twoPi;
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
    static constexpr int numPartials = 16;

    struct Oscillator
    {
        double currentPhase[numPartials] { 0.0 };
        double angleDelta[numPartials]   { 0.0 };
        int waveform { 0 };
        float detune { 0.0f };
        float mix { 0.333f };
    } oscs[3];

    double frequency { 0.0 };
    float level { 0.0f };
    float stretchAmount { 0.0f };

    // Filter
    juce::dsp::StateVariableTPTFilter<float> filter;
    float filterCutoff { 5000.0f };
    float filterResonance { 1.0f };
    int filterType { 0 };

    // LFO
    double lfoPhase { 0.0 };
    double lfoAngleDelta { 0.0 };
    float lfoRate { 1.0f };
    float lfoDepth { 0.0f };
    int lfoShape { 0 };

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;

    float getLFOValue()
    {
        float t = static_cast<float> (lfoPhase / juce::MathConstants<double>::twoPi);
        switch (lfoShape)
        {
            case 0:  // Sine
                return std::sin (static_cast<float> (lfoPhase));
            case 1:  // Square
                return t < 0.5f ? 1.0f : -1.0f;
            case 2:  // Triangle
                return t < 0.5f ? -1.0f + 4.0f * t : 3.0f - 4.0f * t;
            default:
                return 0.0f;
        }
    }
};
