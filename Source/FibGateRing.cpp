#include "FibGateRing.h"
#include "FiLook.h"

FibGateRing::FibGateRing (FiSynthAudioProcessor& p)
    : processor (p)
{
    using APVTS = juce::AudioProcessorValueTreeState;

    genPar = processor.apvts.getRawParameterValue ("gateGen");
    onPar  = processor.apvts.getRawParameterValue ("gateOn");

    onButton.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (onButton);
    onAttachment = std::make_unique<APVTS::ButtonAttachment> (processor.apvts, "gateOn", onButton);

    depthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 14);
    addAndMakeVisible (depthSlider);
    depthAttachment = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "gateDepth", depthSlider);
    depthSlider.onRightClick = [this] { showMidiLearnMenu (processor, "gateDepth"); };

    // Etykiety ze wspólnej tabeli podziałów — indeksy 1:1 z parametrem gateDiv.
    const auto divLabels = FiSynthAudioProcessor::divisionLabels();
    for (int i = 0; i < divLabels.size(); ++i)
        divBox.addItem (divLabels[i], i + 1);
    addAndMakeVisible (divBox);
    divAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "gateDiv", divBox);

    for (int i = 0; i < 5; ++i)
        genBox.addItem (juce::String (FiSynthAudioProcessor::fibonacciWord (i).len), i + 1);
    addAndMakeVisible (genBox);
    genAttachment = std::make_unique<APVTS::ComboBoxAttachment> (processor.apvts, "gateGen", genBox);

    startTimerHz (30);
}

FibGateRing::~FibGateRing()
{
    stopTimer();
}

void FibGateRing::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto row1 = area.removeFromBottom (20);
    auto row2 = area.removeFromBottom (20);

    // Dolny wiersz: podział + długość; wyżej: włącznik + głębokość.
    divBox.setBounds (row1.removeFromLeft (64).reduced (1));
    genBox.setBounds (row1.removeFromLeft (56).reduced (1));

    onButton.setBounds (row2.removeFromLeft (58));
    depthSlider.setBounds (row2.reduced (1));
}

juce::Rectangle<float> FibGateRing::ringBounds() const
{
    auto b = getLocalBounds().toFloat().reduced (8.0f)
        .withTrimmedTop (18.0f).withTrimmedBottom (44.0f);
    const float side = juce::jmin (b.getWidth(), b.getHeight());
    return b.withSizeKeepingCentre (side, side);
}

int FibGateRing::stepNear (juce::Point<float> pos) const
{
    const auto b = ringBounds();
    const auto c = b.getCentre();
    const float R = b.getWidth() * 0.5f - 8.0f;

    const float d = c.getDistanceFrom (pos);
    if (std::abs (d - R) > 12.0f)
        return -1;

    const auto word = FiSynthAudioProcessor::fibonacciWord ((int) genPar->load());
    float ang = std::atan2 (pos.x - c.x, c.y - pos.y);   // 0 na górze, zgodnie z zegarem
    if (ang < 0.0f)
        ang += juce::MathConstants<float>::twoPi;

    return juce::roundToInt (ang / juce::MathConstants<float>::twoPi * (float) word.len) % word.len;
}

void FibGateRing::mouseDown (const juce::MouseEvent& e)
{
    const int step = stepNear (e.position);
    if (step < 0)
        return;

    processor.gateFlips.fetch_xor ((juce::uint64) 1 << step);
    repaint();
}

void FibGateRing::timerCallback()
{
    const int  step  = processor.gateStep.load();
    const auto flips = processor.gateFlips.load();
    const int  gen   = (int) genPar->load();

    if (step != lastStep || flips != lastFlips || gen != lastGen)
    {
        lastStep  = step;
        lastFlips = flips;
        lastGen   = gen;
        repaint();
    }
}

void FibGateRing::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff120d0a));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("GATE FIB \xc2\xb7 S\xe2\x86\x92SL, L\xe2\x86\x92S")),
                getLocalBounds().reduced (8).removeFromTop (16),
                juce::Justification::centredLeft);

    const auto b = ringBounds();
    const auto c = b.getCentre();
    const float R = b.getWidth() * 0.5f - 8.0f;

    const auto word  = FiSynthAudioProcessor::fibonacciWord ((int) genPar->load());
    const auto flips = processor.gateFlips.load();
    const int  cur   = processor.gateStep.load();
    const bool on    = onPar->load() > 0.5f;

    g.setColour (juce::Colour (0x30ffffff));
    g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);

    const auto gold = fiCol::gold;

    for (int i = 0; i < word.len; ++i)
    {
        const float ang = (float) i / (float) word.len * juce::MathConstants<float>::twoPi;
        const auto  pos = c.getPointOnCircumference (R, ang);
        const bool  open  = (((word.bits ^ flips) >> i) & 1) != 0;
        const bool  isCur = (i == cur);

        if (open)
        {
            g.setColour (isCur ? juce::Colours::white : gold.withAlpha (on ? 1.0f : 0.55f));
            const float r = isCur ? 6.0f : 4.5f;
            g.fillEllipse (pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (isCur ? juce::Colours::white : juce::Colour (0x77ffffff));
            const float r = isCur ? 5.5f : 4.0f;
            g.drawEllipse (pos.x - r, pos.y - r, r * 2.0f, r * 2.0f, 1.3f);
        }
    }

    // Wskazówka bieżącego kroku (tylko gdy gate aktywny).
    if (on && cur >= 0)
    {
        const float ang = (float) cur / (float) word.len * juce::MathConstants<float>::twoPi;
        g.setColour (gold.withAlpha (0.5f));
        g.drawLine (juce::Line<float> (c, c.getPointOnCircumference (R - 10.0f, ang)), 1.4f);
    }

    g.setColour (juce::Colour (0x88ffffff));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String (word.len) + juce::String (juce::CharPointer_UTF8 (" krok\xc3\xb3w \xc2\xb7 klik = flip")),
                b.withSizeKeepingCentre (b.getWidth(), 14.0f),
                juce::Justification::centred);
}
