#include "SpectrumAnalyzer.h"
#include "PartialTables.h"
#include "FiLook.h"

SpectrumAnalyzer::SpectrumAnalyzer (FiSynthAudioProcessor& p)
    : processor (p)
{
    phiGridButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xcf\x86\xe2\x81\xbf")));
    octGridButton.setButtonText ("oct");
    for (auto* b : { &phiGridButton, &octGridButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (2002);
        b->setColour (juce::TextButton::buttonOnColourId, fiCol::goldDim);
        addAndMakeVisible (*b);
    }
    phiGridButton.setToggleState (true, juce::dontSendNotification);
    phiGridButton.onClick = [this] { phiGrid = true;  repaint(); };
    octGridButton.onClick = [this] { phiGrid = false; repaint(); };

    startTimerHz (30);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::resized()
{
    auto row = getLocalBounds().reduced (8).removeFromTop (18);
    octGridButton.setBounds (row.removeFromRight (36));
    phiGridButton.setBounds (row.removeFromRight (32));
}

int SpectrumAnalyzer::activeNote() const
{
    return processor.firstActiveNote();
}

void SpectrumAnalyzer::timerCallback()
{
    // Dolej wszystko, co audio odłożyło do FIFO, do pierścienia.
    float tmp[1024];
    bool gotNew = false;
    for (;;)
    {
        const int n = processor.readAnalyzerSamples (tmp, (int) std::size (tmp));
        if (n == 0)
            break;
        gotNew = true;
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) ringPos] = tmp[i];
            ringPos = (ringPos + 1) % fftSize;
        }
    }

    // FFT ostatnich fftSize próbek (rozwinięcie pierścienia od ringPos).
    if (gotNew)
    {
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
    }

    // Peak-decay niezależnie od nowych danych — po ciszy obraz opada do zera.
    bool changed = gotNew;
    for (size_t b = 0; b < smoothDb.size(); ++b)
    {
        const float mag = gotNew ? fftData[b] * (2.0f / (float) fftSize) : 0.0f;
        const float db  = juce::Decibels::gainToDecibels (mag, -100.0f);
        const float next = juce::jmax (db, smoothDb[b] - 1.2f);
        if (std::abs (next - smoothDb[b]) > 0.01f)
        {
            smoothDb[b] = next;
            changed = true;
        }
    }

    if (changed)
        repaint();
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (fiCol::widget);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("WIDMO \xc2\xb7 siatka "))
                    + (phiGrid ? juce::String (juce::CharPointer_UTF8 ("\xcf\x86\xe2\x81\xbf"))
                               : juce::String ("oktawy")),
                area.removeFromTop (22.0f).withTrimmedLeft (8.0f),
                juce::Justification::centredLeft);

    auto plot = area.reduced (6.0f).withTrimmedBottom (12.0f);

    double sr = processor.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;

    const float fMin = 25.0f;
    const float fMax = juce::jmin (20000.0f, (float) sr * 0.5f);
    auto xOf = [&] (float f)
    {
        return plot.getX() + plot.getWidth() * std::log (f / fMin) / std::log (fMax / fMin);
    };

    // Siatka: φⁿ od granej nuty albo oktawy od niej.
    const double f0 = juce::MidiMessage::getMidiNoteInHertz (activeNote());
    {
        const double step = phiGrid ? fiPhi : 2.0;
        g.setColour (phiGrid ? juce::Colour (0x50e0b64b) : juce::Colour (0x3fffffff));
        for (double f = f0; f < fMax; f *= step)
            if (f >= fMin)
                g.drawVerticalLine ((int) xOf ((float) f), plot.getY(), plot.getBottom());
    }

    // Krzywa widma: max z binów przypadających na piksel, -90..0 dB.
    juce::Path path;
    bool started = false;
    const float binHz = (float) sr / (float) fftSize;

    for (float x = 0.0f; x <= plot.getWidth(); x += 2.0f)
    {
        const float fA = fMin * std::pow (fMax / fMin, x / plot.getWidth());
        const float fB = fMin * std::pow (fMax / fMin, juce::jmin (1.0f, (x + 2.0f) / plot.getWidth()));
        const int bA = juce::jlimit (1, fftSize / 2 - 1, (int) (fA / binHz));
        const int bB = juce::jlimit (bA, fftSize / 2 - 1, (int) (fB / binHz));

        float db = -100.0f;
        for (int b = bA; b <= bB; ++b)
            db = juce::jmax (db, smoothDb[(size_t) b]);

        const float norm = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);
        const float y = plot.getBottom() - norm * plot.getHeight();

        if (! started) { path.startNewSubPath (plot.getX() + x, y); started = true; }
        else             path.lineTo (plot.getX() + x, y);
    }

    if (started)
    {
        juce::Path fill = path;
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xaae0b64b), plot.getX(), plot.getY(),
                                                 juce::Colour (0x10e0b64b), plot.getX(), plot.getBottom(),
                                                 false));
        g.fillPath (fill);
        g.setColour (fiCol::gold);
        g.strokePath (path, juce::PathStrokeType (1.4f));
    }

    g.setColour (juce::Colour (0x66ffffff));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText (juce::MidiMessage::getMidiNoteName (activeNote(), true, true, 4),
                juce::Rectangle<float> (plot.getX(), plot.getBottom() + 1.0f, 60.0f, 11.0f),
                juce::Justification::left);
    g.drawText ("25 Hz", juce::Rectangle<float> (plot.getX(), plot.getBottom() + 1.0f, plot.getWidth(), 11.0f),
                juce::Justification::centred);
    g.drawText ("20 k", juce::Rectangle<float> (plot.getRight() - 40.0f, plot.getBottom() + 1.0f, 40.0f, 11.0f),
                juce::Justification::right);
}
