#pragma once

#include <JuceHeader.h>

// === Panel "About" — dokumentacja syntezatora w pluginie ===
//
// Pełnoekranowa nakładka (toggle przyciskiem About w górnym pasku): po lewej
// menu sekcji, po prawej przewijany opis. Treść opisuje tor sygnału, każdy
// moduł, wzory trybów stretcha, architekturę wątków i zależności buildu —
// jedno miejsce prawdy o tym, JAK i DLACZEGO FiSynth działa.
// Przycisk PL/EN w nagłówku przełącza język całej treści w locie.
class AboutPanel : public juce::Component
{
public:
    AboutPanel();

    void paint (juce::Graphics&) override;
    void resized() override;

    // Klik w tło poza kartą też zamyka panel.
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showSection (int idx);
    void setLanguage (bool en);

    struct Section { juce::String title, body; };
    static std::vector<Section> makeSections (bool english);

    bool english { false };
    int currentSection { 0 };
    std::vector<Section> sections;
    juce::OwnedArray<juce::TextButton> navButtons;
    juce::TextButton closeButton, langButton;
    juce::TextEditor content;
    juce::Rectangle<int> card;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};
