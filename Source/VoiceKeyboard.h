#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"

// === Klawiatura ekranowa z podświetlaniem grających głosów ===
//
// Nuty klikane myszą trafiają przez keyboardState procesora do syntezy tak,
// jakby przyszły z MIDI (processNextMidiBuffer w processBlock). Klawisze
// aktualnie grających głosów podświetlamy kolorem slotu playheada — ta sama
// paleta co kropki w edytorze obwiedni, więc głos ma jeden kolor w całym GUI.
//
// MidiKeyboardComponent sam dziedziczy (prywatnie) po Timer, więc odpytywanie
// envPlayheadNote robi wewnętrzny timer-składnik zamiast drugiej bazy Timer.
class VoiceKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit VoiceKeyboard (FiSynthAudioProcessor& p)
        : juce::MidiKeyboardComponent (p.keyboardState,
                                       juce::MidiKeyboardComponent::horizontalKeyboard),
          processor (p)
    {
        setAvailableRange (36, 96);      // C2..C7 (36 białych klawiszy)
        setOctaveForMiddleC (4);         // nazwy nut jak w legendzie playheadów
        setScrollButtonsVisible (false); // pełny zakres mieści się w oknie

        for (auto& n : lastNotes)
            n = -1;

        poll.owner = this;
        poll.startTimerHz (30);
    }

protected:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (midiNoteNumber, g, area,
                                                    isDown, isOver, lineColour, textColour);
        tintPlayingNote (midiNoteNumber, g, area);
    }

    void drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (midiNoteNumber, g, area,
                                                    isDown, isOver, noteFillColour);
        tintPlayingNote (midiNoteNumber, g, area);
    }

private:
    // Slot playheada grający daną nutę (-1 = żaden) — z niego bierzemy kolor.
    int slotForNote (int note) const noexcept
    {
        if (note < 0)
            return -1;

        for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
            if (processor.envPlayheadNote[i].load() == note)
                return i;
        return -1;
    }

    void tintPlayingNote (int note, juce::Graphics& g, juce::Rectangle<float> area)
    {
        const int slot = slotForNote (note);
        if (slot < 0)
            return;

        const auto col = EnvelopeEditor::playheadColour (slot);
        g.setColour (col.withAlpha (0.40f));
        g.fillRect (area);
        g.setColour (col);
        g.fillRect (area.removeFromBottom (4.0f));
    }

    struct Poll : juce::Timer
    {
        VoiceKeyboard* owner = nullptr;
        void timerCallback() override { owner->pollNotes(); }
    };

    void pollNotes()
    {
        bool changed = false;
        for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
        {
            const int n = processor.envPlayheadNote[i].load();
            if (n != lastNotes[i])
            {
                lastNotes[i] = n;
                changed = true;
            }
        }
        if (changed)
            repaint();
    }

    FiSynthAudioProcessor& processor;
    Poll poll;
    int  lastNotes[FiSynthAudioProcessor::kMaxPlayheads] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceKeyboard)
};
