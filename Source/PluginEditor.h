#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"
#include "VoiceKeyboard.h"
#include "ModRingOverlay.h"
#include "SpiralVisualizer.h"
#include "SpectrumAnalyzer.h"
#include "FibGateRing.h"
#include "PhylloField.h"
#include "AboutPanel.h"
#include "FiLook.h"

class FiSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FiSynthAudioProcessorEditor (FiSynthAudioProcessor&);
    ~FiSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FiSynthAudioProcessor& processorRef;

    // LookAndFeel (złoto na ciepłej czerni) — PIERWSZY składnik: niszczony
    // ostatni, więc żaden komponent nie przeżywa swojego look&feel.
    FiLookAndFeel fiLnF;

    // Płótno o stałym układzie bazowym (baseWidth×baseHeight): wszystkie
    // kontrolki są jego dziećmi, a edytor — resizowalny — skaluje całość
    // transformem w resized(). Layout liczy się zawsze w przestrzeni bazowej
    // (layoutContent), tło sekcji rysuje paintContent.
    struct Canvas : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override
        {
            if (onPaint != nullptr)
                onPaint (g);
        }
    };
    Canvas content;

    static constexpr int baseWidth = 1460, baseHeight = 940;
    void layoutContent();
    void paintContent (juce::Graphics&);

    // Section bounds dla teł sekcji (używane w paint). Układ "hero-spirala":
    // oscylatory w lewej kolumnie (3 panele), spirala-kontroler w centrum,
    // modulacja/filtr/LFO/φ-engine po prawej, pas gate|widmo|stereo na dole.
    juce::Rectangle<int> gainBounds, oscPanelBounds[3], envBounds, filterBounds,
                         lfoBounds, goldBounds, spiralBounds, gateBounds,
                         phylloBounds, spectrumBounds;

    juce::Slider gainSlider;
    juce::Label gainLabel;

    // Graficzna obwiednia + routing modulacji (źródło = obwiednia)
    EnvelopeEditor envEditor;
    juce::Label    envTitleLabel;

    // Zakładki wyboru aktywnej obwiedni (Amp / 1 / 2 / 3).
    juce::TextButton envTabs[kNumEnvelopes];
    void selectEnvelope (int idx);

    // Narzędzia φ aktywnej obwiedni: generator kaskady 1/φᵏ (samopodobny decay)
    // i skalowanie czasów wszystkich punktów złotym stosunkiem.
    juce::TextButton phiCascadeButton, phiMulButton, phiDivButton;
    void applyPhiCascade();
    void scaleActiveEnvelopeTimes (float factor);

    // Kwantyzacja modulacji Pitch do siatki złotego interwału (833.09¢).
    juce::ToggleButton pitchQuantButton;

    // Klawiatura ekranowa (nuty myszą; klawisze świecą kolorami playheadów).
    VoiceKeyboard keyboard;

    // Wizualizery i moduły φ. Spirala = centrum: wizualizer partiali
    // i kontroler stretch×tryb w jednym (wchłonęła dawny MorphPad).
    SpiralVisualizer spiral;
    SpectrumAnalyzer spectrum;
    FibGateRing      fibGate;
    PhylloField      phyllo;

    // Panel About (pełnoekranowa nakładka) + przycisk w górnym pasku.
    AboutPanel       aboutPanel;
    juce::TextButton aboutButton { "About" };

    // === Pasek presetów (zapis/wczytywanie pełnego brzmienia) ===
    juce::ComboBox   presetBox;
    juce::TextButton presetNewButton  { "New" };
    juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, presetSaveButton { "Save" };
    void refreshPresetList();
    void loadPresetByName (const juce::String& name);
    void stepPreset (int delta);
    void newPreset();
    void showSavePresetDialog();

    // Synchronizacja obwiedni do tempa: sync, podział siatki, snap, ręczne BPM.
    // tempoSyncButton wybiera źródło tempa: DAW (wł.) vs ręczne BPM (wył.).
    juce::ToggleButton tempoSyncButton, envSyncButton, envSnapButton;
    juce::ComboBox     envGridBox;
    juce::Slider       bpmSlider;
    juce::Label        bpmLabel;
    void updateBpmEnablement();

    struct ModSlotControl
    {
        juce::ComboBox destBox;
        juce::Slider   amtSlider;
        juce::Label    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> destAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAttachment;
    } modSlots[3];

    // 3 Oscylatory. Stretch i tryb NIE mają tu kontrolek — steruje nimi
    // spirala-kontroler (drag: promień = stretch, kąt = tryb; klik w etykietę
    // trybu = dawny ComboBox).
    struct OscControl
    {
        juce::ComboBox waveformBox;
        juce::Slider detuneSlider;
        juce::Slider mixSlider;
        juce::Slider tiltSlider;
        juce::ComboBox goldIntBox;
        juce::Label waveformLabel, detuneLabel, mixLabel, tiltLabel, goldIntLabel;

        using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::unique_ptr<ComboBoxAttachment> waveformAttachment;
        std::unique_ptr<SliderAttachment> detuneAttachment;
        std::unique_ptr<SliderAttachment> mixAttachment;
        std::unique_ptr<SliderAttachment> tiltAttachment;
        std::unique_ptr<ComboBoxAttachment> goldIntAttachment;
    } oscs[3];

    // Filter
    juce::Slider filterCutoffSlider, filterResonanceSlider;
    juce::ComboBox filterTypeBox;
    juce::Label filterCutoffLabel, filterResonanceLabel, filterTypeLabel;

    // LFO
    juce::Slider lfoRateSlider, lfoDepthSlider, lfoDriftSlider;
    juce::ComboBox lfoShapeBox;
    juce::Label lfoRateLabel, lfoDepthLabel, lfoShapeLabel, lfoDriftLabel;
    juce::ToggleButton lfoSyncButton;   // wł. = rate z podziału nut pod tempo
    juce::ComboBox     lfoRateDivBox;
    void updateLfoSyncEnablement();

    // === Sekcja GOLD: złoty silnik (Ring/FM/Sub/Unison) + Golden Delay ===
    juce::Slider ringSlider, fmSlider, subSlider, uniSlider;
    juce::Label  ringLabel, fmLabel, subLabel, uniLabel;
    juce::ToggleButton dlyOnButton, dlySyncButton;
    juce::ComboBox     dlyDivBox;
    juce::Slider       dlyTimeSlider, dlyFbSlider, dlyMixSlider;
    juce::Label        dlyFbLabel, dlyMixLabel;
    void updateDelaySyncEnablement();

    // Arpeggiator Fibonacciego (górny pasek, obok kontrolek tempa).
    juce::ToggleButton arpOnButton, arpWordButton;
    juce::ComboBox     arpDivBox, arpLenBox, arpModeBox;
    juce::Slider       arpVelSlider;
    juce::Label        arpVelLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment>   tempoSyncAttachment, envSyncAttachment, envSnapAttachment;
    std::unique_ptr<ComboBoxAttachment> envGridAttachment;
    std::unique_ptr<SliderAttachment>   bpmAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterResonanceAttachment;
    std::unique_ptr<ComboBoxAttachment> filterTypeAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<ComboBoxAttachment> lfoShapeAttachment;
    std::unique_ptr<ButtonAttachment>   lfoSyncAttachment;
    std::unique_ptr<ComboBoxAttachment> lfoRateDivAttachment;
    std::unique_ptr<SliderAttachment>   lfoDriftAttachment;
    std::unique_ptr<SliderAttachment>   ringAttachment, fmAttachment,
                                        subAttachment, uniAttachment;
    std::unique_ptr<ButtonAttachment>   pitchQuantAttachment;
    std::unique_ptr<ButtonAttachment>   dlyOnAttachment, dlySyncAttachment;
    std::unique_ptr<ComboBoxAttachment> dlyDivAttachment;
    std::unique_ptr<SliderAttachment>   dlyTimeAttachment, dlyFbAttachment, dlyMixAttachment;
    std::unique_ptr<ButtonAttachment>   arpOnAttachment, arpWordAttachment;
    std::unique_ptr<ComboBoxAttachment> arpDivAttachment, arpLenAttachment, arpModeAttachment;
    std::unique_ptr<SliderAttachment>   arpVelAttachment;

    // Nakładka pierścieni modulacji trzyma surowe wskaźniki do sliderów-celów,
    // więc jest zadeklarowana OSTATNIA — niszczona PRZED sliderami (odwrotna
    // kolejność destrukcji), nigdy nie przeżywa swoich celów.
    std::unique_ptr<ModRingOverlay> modRings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessorEditor)
};
