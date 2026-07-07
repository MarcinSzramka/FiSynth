#include "PhylloField.h"
#include "PartialTables.h"

PhylloField::PhylloField (FiSynthAudioProcessor& p)
    : processor (p)
{
    spreadPar = processor.apvts.getRawParameterValue ("spread");

    spreadSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    spreadSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 14);
    addAndMakeVisible (spreadSlider);
    spreadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "spread", spreadSlider);

    spreadLabel.setText ("Spread", juce::dontSendNotification);
    spreadLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (spreadLabel);

    startTimerHz (30);
}

PhylloField::~PhylloField()
{
    stopTimer();
}

void PhylloField::resized()
{
    auto row = getLocalBounds().reduced (8).removeFromBottom (20);
    spreadLabel.setBounds (row.removeFromLeft (52));
    spreadSlider.setBounds (row.reduced (1));
}

void PhylloField::timerCallback()
{
    const float s = spreadPar->load();
    if (! juce::exactlyEqual (s, lastSpread))
    {
        lastSpread = s;
        repaint();
    }
}

void PhylloField::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff0a1210));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("STEREO \xc2\xb7 pan phyllotaxis")),
                getLocalBounds().reduced (8).removeFromTop (16),
                juce::Justification::centredLeft);

    auto plot = getLocalBounds().toFloat().reduced (10.0f)
        .withTrimmedTop (20.0f).withTrimmedBottom (28.0f);

    g.setColour (juce::Colour (0xff0e0a14));
    g.fillRect (plot);
    g.setColour (juce::Colour (0xff2a2e2b));
    g.drawRect (plot, 1.0f);

    // Oś środka (mono) + etykiety L/R.
    g.setColour (juce::Colour (0x28ffffff));
    g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());
    g.setColour (juce::Colour (0x77ffffff));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("L", plot.reduced (4.0f), juce::Justification::bottomLeft);
    g.drawText ("R", plot.reduced (4.0f), juce::Justification::bottomRight);

    const float spread = spreadPar->load();

    // fiGoldenPan — dokładnie ta funkcja, którą panoramuje SynthVoice.
    for (int n = 0; n < fiNumPartials; ++n)
    {
        const float pan = fiGoldenPan (n) * spread;

        const float x = plot.getCentreX() + pan * plot.getWidth() * 0.48f;
        const float y = plot.getBottom() - (plot.getHeight() - 10.0f) * ((float) n + 0.5f) / (float) fiNumPartials;
        const float amp = fiHarmonicAmplitude (3, n);   // saw jako reprezentatywna obwiednia widma
        const float r = 1.8f + 4.5f * std::pow (amp, 0.4f);

        g.setColour (juce::Colour (0xff5ec4bc).withAlpha (0.35f + 0.65f * amp));
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }

    g.setColour (juce::Colour (0x66ffffff));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("partial n \xe2\x86\x92 pan frac(n/\xcf\x86)\xc2\xb7spread")),
                juce::Rectangle<float> (plot.getX(), plot.getY() - 2.0f, plot.getWidth(), 12.0f),
                juce::Justification::centredRight);
}
