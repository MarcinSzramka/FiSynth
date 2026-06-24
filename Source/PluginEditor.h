#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EnvelopeEditor.h"

class FiSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FiSynthAudioProcessorEditor (FiSynthAudioProcessor&);
    ~FiSynthAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FiSynthAudioProcessor& processorRef;

    // Section bounds dla kolorowych tła (używane w paint)
    juce::Rectangle<int> gainBounds, oscBounds, envBounds, filterBounds, lfoBounds;

    juce::Slider gainSlider;
    juce::Label gainLabel;

    // Graficzna obwiednia + routing modulacji (źródło = obwiednia)
    EnvelopeEditor envEditor;
    juce::Label    envTitleLabel;

    // Zakładki wyboru aktywnej obwiedni (Amp / 1 / 2 / 3).
    juce::TextButton envTabs[kNumEnvelopes];
    void selectEnvelope (int idx);

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

    // 3 Oscylatory
    struct OscControl
    {
        juce::ComboBox waveformBox;
        juce::Slider detuneSlider;
        juce::Slider mixSlider;
        juce::Slider stretchSlider;
        juce::Label waveformLabel, detuneLabel, mixLabel, stretchLabel;

        using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::unique_ptr<ComboBoxAttachment> waveformAttachment;
        std::unique_ptr<SliderAttachment> detuneAttachment;
        std::unique_ptr<SliderAttachment> mixAttachment;
        std::unique_ptr<SliderAttachment> stretchAttachment;
    } oscs[3];

    // Filter
    juce::Slider filterCutoffSlider, filterResonanceSlider;
    juce::ComboBox filterTypeBox;
    juce::Label filterCutoffLabel, filterResonanceLabel, filterTypeLabel;

    // LFO
    juce::Slider lfoRateSlider, lfoDepthSlider;
    juce::ComboBox lfoShapeBox;
    juce::Label lfoRateLabel, lfoDepthLabel, lfoShapeLabel;
    juce::ToggleButton lfoSyncButton;   // wł. = rate z podziału nut pod tempo
    juce::ComboBox     lfoRateDivBox;
    void updateLfoSyncEnablement();

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessorEditor)
};
