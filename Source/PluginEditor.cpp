#include "PluginEditor.h"

FiSynthAudioProcessorEditor::FiSynthAudioProcessorEditor (FiSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Gain + Stretch
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (gainSlider);
    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);
    gainAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "gain", gainSlider);

    // ADSR
    auto setupADSRSlider = [this] (juce::Slider& slider, juce::Label& label, const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
        addAndMakeVisible (slider);
        label.setText (name, juce::dontSendNotification);
        addAndMakeVisible (label);
    };

    setupADSRSlider (attackSlider, attackLabel, "Attack");
    setupADSRSlider (decaySlider, decayLabel, "Decay");
    setupADSRSlider (sustainSlider, sustainLabel, "Sustain");
    setupADSRSlider (releaseSlider, releaseLabel, "Release");

    attackAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "attack", attackSlider);
    decayAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "decay", decaySlider);
    sustainAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "sustain", sustainSlider);
    releaseAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "release", releaseSlider);

    // 3 Oscylatory
    for (int o = 0; o < 3; ++o)
    {
        juce::String prefix = "osc" + juce::String (o + 1);

        oscs[o].waveformBox.addItem ("Sine", 1);
        oscs[o].waveformBox.addItem ("Square", 2);
        oscs[o].waveformBox.addItem ("Triangle", 3);
        oscs[o].waveformBox.addItem ("Sawtooth", 4);
        oscs[o].waveformBox.addItem ("Quadratic", 5);
        oscs[o].waveformBox.addItem ("Noise", 6);
        addAndMakeVisible (oscs[o].waveformBox);
        oscs[o].waveformLabel.setText (prefix + " Wave", juce::dontSendNotification);
        addAndMakeVisible (oscs[o].waveformLabel);
        oscs[o].waveformAttachment = std::make_unique<OscControl::ComboBoxAttachment> (
            processorRef.apvts, prefix + "waveform", oscs[o].waveformBox);

        oscs[o].detuneSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        oscs[o].detuneSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
        addAndMakeVisible (oscs[o].detuneSlider);
        oscs[o].detuneLabel.setText (prefix + " Detune", juce::dontSendNotification);
        addAndMakeVisible (oscs[o].detuneLabel);
        oscs[o].detuneAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "detune", oscs[o].detuneSlider);

        oscs[o].mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        oscs[o].mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
        addAndMakeVisible (oscs[o].mixSlider);
        oscs[o].mixLabel.setText (prefix + " Mix", juce::dontSendNotification);
        addAndMakeVisible (oscs[o].mixLabel);
        oscs[o].mixAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "mix", oscs[o].mixSlider);

        oscs[o].stretchSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        oscs[o].stretchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
        addAndMakeVisible (oscs[o].stretchSlider);
        oscs[o].stretchLabel.setText (prefix + " Stretch", juce::dontSendNotification);
        addAndMakeVisible (oscs[o].stretchLabel);
        oscs[o].stretchAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "stretch", oscs[o].stretchSlider);
    }

    // Filter
    filterCutoffSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterCutoffSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 16);
    addAndMakeVisible (filterCutoffSlider);
    filterCutoffLabel.setText ("Filter Cutoff", juce::dontSendNotification);
    addAndMakeVisible (filterCutoffLabel);
    filterCutoffAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "filterCutoff", filterCutoffSlider);

    filterResonanceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterResonanceSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    addAndMakeVisible (filterResonanceSlider);
    filterResonanceLabel.setText ("Resonance", juce::dontSendNotification);
    addAndMakeVisible (filterResonanceLabel);
    filterResonanceAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "filterResonance", filterResonanceSlider);

    filterTypeBox.addItem ("Low-pass", 1);
    filterTypeBox.addItem ("Band-pass", 2);
    filterTypeBox.addItem ("High-pass", 3);
    addAndMakeVisible (filterTypeBox);
    filterTypeLabel.setText ("Type", juce::dontSendNotification);
    addAndMakeVisible (filterTypeLabel);
    filterTypeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "filterType", filterTypeBox);

    // LFO
    lfoRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    addAndMakeVisible (lfoRateSlider);
    lfoRateLabel.setText ("LFO Rate", juce::dontSendNotification);
    addAndMakeVisible (lfoRateLabel);
    lfoRateAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "lfoRate", lfoRateSlider);

    lfoDepthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    addAndMakeVisible (lfoDepthSlider);
    lfoDepthLabel.setText ("LFO Depth", juce::dontSendNotification);
    addAndMakeVisible (lfoDepthLabel);
    lfoDepthAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "lfoDepth", lfoDepthSlider);

    lfoShapeBox.addItem ("Sine", 1);
    lfoShapeBox.addItem ("Square", 2);
    lfoShapeBox.addItem ("Triangle", 3);
    addAndMakeVisible (lfoShapeBox);
    lfoShapeLabel.setText ("LFO Shape", juce::dontSendNotification);
    addAndMakeVisible (lfoShapeLabel);
    lfoShapeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "lfoShape", lfoShapeBox);

    setSize (900, 650);
}

void FiSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));

    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    g.drawText ("FiSynth", 10, 5, 200, 30, juce::Justification::left);
}

void FiSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (5);

    // Gain
    auto topRow = area.removeFromTop (80);
    gainLabel.setBounds (topRow.removeFromTop (20));
    gainSlider.setBounds (topRow.withSizeKeepingCentre (100, 100));

    // ADSR
    auto adsrRow = area.removeFromTop (20);
    int adsrW = adsrRow.getWidth() / 4;
    attackLabel.setBounds (adsrRow.removeFromLeft (adsrW));
    decayLabel.setBounds (adsrRow.removeFromLeft (adsrW));
    sustainLabel.setBounds (adsrRow.removeFromLeft (adsrW));
    releaseLabel.setBounds (adsrRow.removeFromLeft (adsrW));

    adsrRow = area.removeFromTop (24);
    adsrW = adsrRow.getWidth() / 4;
    attackSlider.setBounds (adsrRow.removeFromLeft (adsrW).reduced (5, 0));
    decaySlider.setBounds (adsrRow.removeFromLeft (adsrW).reduced (5, 0));
    sustainSlider.setBounds (adsrRow.removeFromLeft (adsrW).reduced (5, 0));
    releaseSlider.setBounds (adsrRow.removeFromLeft (adsrW).reduced (5, 0));

    // Filter
    auto filterRow = area.removeFromTop (20);
    filterCutoffLabel.setBounds (filterRow.removeFromLeft (200));
    filterResonanceLabel.setBounds (filterRow.removeFromLeft (100));
    filterTypeLabel.setBounds (filterRow.removeFromLeft (80));

    filterRow = area.removeFromTop (24);
    filterCutoffSlider.setBounds (filterRow.removeFromLeft (200).reduced (5, 0));
    filterResonanceSlider.setBounds (filterRow.removeFromLeft (100).reduced (5, 0));
    filterTypeBox.setBounds (filterRow.removeFromLeft (80).reduced (5, 0));

    // LFO
    auto lfoRow = area.removeFromTop (20);
    lfoRateLabel.setBounds (lfoRow.removeFromLeft (150));
    lfoDepthLabel.setBounds (lfoRow.removeFromLeft (150));
    lfoShapeLabel.setBounds (lfoRow.removeFromLeft (100));

    lfoRow = area.removeFromTop (24);
    lfoRateSlider.setBounds (lfoRow.removeFromLeft (150).reduced (5, 0));
    lfoDepthSlider.setBounds (lfoRow.removeFromLeft (150).reduced (5, 0));
    lfoShapeBox.setBounds (lfoRow.removeFromLeft (100).reduced (5, 0));

    // 3 Oscylatory
    for (int o = 0; o < 3; ++o)
    {
        auto oscArea = area.removeFromTop (70);

        oscs[o].waveformLabel.setBounds (oscArea.removeFromTop (16));
        oscs[o].waveformBox.setBounds (oscArea.removeFromLeft (150).removeFromTop (24));

        oscs[o].detuneLabel.setBounds (oscArea.removeFromTop (16));
        oscs[o].detuneSlider.setBounds (oscArea.removeFromLeft (200).removeFromTop (24).reduced (5, 0));

        auto mixArea = oscArea.removeFromLeft (oscArea.getWidth() / 2);
        oscs[o].mixLabel.setBounds (mixArea.removeFromTop (16));
        oscs[o].mixSlider.setBounds (mixArea.withSizeKeepingCentre (90, 90));

        oscs[o].stretchLabel.setBounds (oscArea.removeFromTop (16));
        oscs[o].stretchSlider.setBounds (oscArea.withSizeKeepingCentre (90, 90));
    }
}
