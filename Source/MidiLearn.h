#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// === MIDI Learn — strona GUI ===
//
// Slider z hakiem na prawy klik. Zwykły juce::Slider przy wyłączonym własnym
// menu traktuje prawy przycisk jak drag (snap wartości do pozycji myszy),
// więc menu MIDI Learn musi połknąć zdarzenie ZANIM baza zacznie drag.
struct LearnSlider : juce::Slider
{
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick != nullptr)
                onRightClick();
            return;
        }
        juce::Slider::mouseDown (e);
    }
};

// Dokłada do menu pozycje MIDI Learn jednego parametru (Learn/anuluj + usuń
// przypisanie). Wspólne dla sliderów i spirali (tam dwa parametry w jednym
// menu). Procesor przeżywa edytor, a menu z akcjami-lambdami jest zamykane
// przy zniszczeniu GUI, więc referencja w lambdach jest bezpieczna.
inline void addMidiLearnItems (juce::PopupMenu& menu, FiSynthAudioProcessor& proc,
                               const juce::String& paramID)
{
    auto* param = proc.apvts.getParameter (paramID);
    if (param == nullptr)
        return;

    const auto name  = param->getName (64);
    const bool armed = proc.isMidiLearnArmed (paramID);
    const int  cc    = proc.ccForParam (paramID);

    if (armed)
        menu.addItem (juce::String (juce::CharPointer_UTF8 ("Learn czeka na CC\xe2\x80\xa6 anuluj: ")) + name,
                      [&proc] { proc.cancelMidiLearn(); });
    else
        menu.addItem ("MIDI Learn: " + name,
                      [&proc, paramID] { proc.startMidiLearn (paramID); });

    if (cc >= 0)
        menu.addItem (juce::String (juce::CharPointer_UTF8 ("Usu\xc5\x84 CC ")) + juce::String (cc) + ": " + name,
                      [&proc, paramID] { proc.clearCcMapping (paramID); });
}

// Menu dla pojedynczej gałki (typowy przypadek LearnSlider::onRightClick).
inline void showMidiLearnMenu (FiSynthAudioProcessor& proc, const juce::String& paramID)
{
    juce::PopupMenu menu;
    addMidiLearnItems (menu, proc, paramID);
    menu.showMenuAsync (juce::PopupMenu::Options());
}
