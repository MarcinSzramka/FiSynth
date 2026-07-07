#include "PluginEditor.h"
#include "PartialTables.h"

FiSynthAudioProcessorEditor::FiSynthAudioProcessorEditor (FiSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), envEditor (p), keyboard (p),
      spiral (p), spectrum (p), fibGate (p), phyllo (p)
{
    // Całe UI mieszka na 'content' (stała przestrzeń baseWidth×baseHeight);
    // edytor skaluje je transformem, dzięki czemu okno można zmieniać rozmiarem.
    // LookAndFeel na płótnie — wszystkie dzieci dziedziczą złotą paletę.
    content.setLookAndFeel (&fiLnF);
    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };

    // Gain + Stretch
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    content.addAndMakeVisible (gainSlider);
    gainLabel.setText ("Volume", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    content.addAndMakeVisible (gainLabel);
    gainAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "gain", gainSlider);

    // Źródło tempa: wł. = synchronizacja z tempem DAW, wył. = ręczne BPM poniżej.
    tempoSyncButton.setButtonText ("Sync Tempo");
    tempoSyncButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Synchronizuj z tempem DAW (wy\xc5\x82. = u\xc5\xbcyj r\xc4\x99""cznego BPM)")));
    content.addAndMakeVisible (tempoSyncButton);
    tempoSyncAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "tempoSync", tempoSyncButton);
    // Gdy synchronizujemy z DAW, ręczny suwak BPM jest nieaktywny.
    tempoSyncButton.onClick = [this] { updateBpmEnablement(); };

    // Graficzna obwiednia
    content.addAndMakeVisible (envEditor);
    envTitleLabel.setText ("Envelope", juce::dontSendNotification);
    content.addAndMakeVisible (envTitleLabel);

    // Zakładki: wybór, która obwiednia jest aktywna do edycji. Wszystkie są
    // rysowane naraz, ale tylko aktywną można edytować.
    for (int e = 0; e < kNumEnvelopes; ++e)
    {
        envTabs[e].setButtonText (e == 0 ? "Amp" : juce::String (e));
        envTabs[e].setClickingTogglesState (true);
        envTabs[e].setRadioGroupId (1001);
        envTabs[e].setColour (juce::TextButton::buttonOnColourId,
                              EnvelopeEditor::envelopeColour (e).withMultipliedBrightness (0.7f));
        envTabs[e].setToggleState (e == envEditor.getActiveEnvelope(), juce::dontSendNotification);
        envTabs[e].onClick = [this, e] { selectEnvelope (e); };
        content.addAndMakeVisible (envTabs[e]);
    }

    // Narzędzia φ obwiedni: kaskada 1/φᵏ + skalowanie czasów złotym stosunkiem.
    phiCascadeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xcf\x86-casc")));
    phiMulButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97\xcf\x86")));
    phiDivButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\xb7\xcf\x86")));
    phiCascadeButton.onClick = [this] { applyPhiCascade(); };
    phiMulButton.onClick     = [this] { scaleActiveEnvelopeTimes ((float) fiPhi); };
    phiDivButton.onClick     = [this] { scaleActiveEnvelopeTimes ((float) (1.0 / fiPhi)); };
    content.addAndMakeVisible (phiCascadeButton);
    content.addAndMakeVisible (phiMulButton);
    content.addAndMakeVisible (phiDivButton);

    // === Pasek presetów ===
    presetBox.setTextWhenNothingSelected (juce::String (juce::CharPointer_UTF8 ("Preset\xe2\x80\xa6")));
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty())
            loadPresetByName (name);
    };
    content.addAndMakeVisible (presetBox);

    presetNewButton.onClick  = [this] { newPreset(); };
    presetPrevButton.onClick = [this] { stepPreset (-1); };
    presetNextButton.onClick = [this] { stepPreset (+1); };
    presetSaveButton.onClick = [this] { showSavePresetDialog(); };
    content.addAndMakeVisible (presetNewButton);
    content.addAndMakeVisible (presetPrevButton);
    content.addAndMakeVisible (presetNextButton);
    content.addAndMakeVisible (presetSaveButton);

    refreshPresetList();

    // Sync do tempa + siatka. Czasy obwiedni stają się beatami i podążają za BPM.
    envSyncButton.setButtonText ("Sync");
    content.addAndMakeVisible (envSyncButton);
    envSyncAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "envSync", envSyncButton);

    // Przy przełączeniu sync przeliczamy czasy punktów wg bieżącego BPM, żeby
    // obwiednia zachowała realną długość: sekundy->beaty (wł.) lub beaty->sekundy
    // (wył.). Atak listenerów: ButtonAttachment używa addListener, nie onClick,
    // więc to nie koliduje z synchronizacją parametru.
    envSyncButton.onClick = [this]
    {
        const bool   nowOn  = envSyncButton.getToggleState();
        const double bpm    = juce::jmax (1.0f, processorRef.currentBpm.load());
        const double factor = nowOn ? (bpm / 60.0)    // sekundy -> beaty
                                    : (60.0 / bpm);   // beaty -> sekundy
        processorRef.convertEnvelopeTimes (factor);
        envEditor.repaint();
    };

    // Etykiety siatki ze wspólnej tabeli podziałów (spójne z gate'em).
    {
        const auto divLabels = FiSynthAudioProcessor::divisionLabels();
        for (int i = 0; i < divLabels.size(); ++i)
            envGridBox.addItem (divLabels[i], i + 1);
    }
    content.addAndMakeVisible (envGridBox);
    envGridAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "envGrid", envGridBox);

    envSnapButton.setButtonText ("Snap");
    content.addAndMakeVisible (envSnapButton);
    envSnapAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "envSnap", envSnapButton);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    content.addAndMakeVisible (bpmLabel);

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 16);
    bpmSlider.setNumDecimalPlacesToDisplay (1);
    bpmSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "R\xc4\x99""czne BPM (u\xc5\xbcywane gdy host/DAW nie podaje tempa)")));
    content.addAndMakeVisible (bpmSlider);
    bpmAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "bpm", bpmSlider);

    // Routing modulacji: obwiednia jako źródło -> 3 sloty (cel + głębokość)
    for (int slot = 0; slot < 3; ++slot)
    {
        juce::String prefix = "env" + juce::String (slot + 1);

        modSlots[slot].destBox.addItem ("None", 1);
        modSlots[slot].destBox.addItem ("Filter Cutoff", 2);
        modSlots[slot].destBox.addItem ("Pitch", 3);
        modSlots[slot].destBox.addItem ("Osc Mix", 4);
        modSlots[slot].destBox.addItem ("Resonance", 5);
        modSlots[slot].destBox.addItem ("Stretch 1", 6);
        modSlots[slot].destBox.addItem ("Stretch 2", 7);
        modSlots[slot].destBox.addItem ("Stretch 3", 8);
        modSlots[slot].destBox.addItem ("FM", 9);
        modSlots[slot].destBox.addItem ("Tilt 1", 10);
        modSlots[slot].destBox.addItem ("Tilt 2", 11);
        modSlots[slot].destBox.addItem ("Tilt 3", 12);
        content.addAndMakeVisible (modSlots[slot].destBox);

        modSlots[slot].amtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        modSlots[slot].amtSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
        content.addAndMakeVisible (modSlots[slot].amtSlider);

        // Slot N jest napędzany obwiednią modulacyjną o tym samym numerze.
        modSlots[slot].label.setText ("Env " + juce::String (slot + 1)
                                          + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92")),
                                      juce::dontSendNotification);
        modSlots[slot].label.setColour (juce::Label::textColourId,
                                        EnvelopeEditor::envelopeColour (slot + 1));
        content.addAndMakeVisible (modSlots[slot].label);

        modSlots[slot].destAttachment = std::make_unique<ComboBoxAttachment> (
            processorRef.apvts, prefix + "Dest", modSlots[slot].destBox);
        modSlots[slot].amtAttachment = std::make_unique<SliderAttachment> (
            processorRef.apvts, prefix + "Amt", modSlots[slot].amtSlider);
    }

    // 3 Oscylatory — stretch i tryb mieszkają na spirali-kontrolerze,
    // tu zostaje barwa: waveform, golden int, detune, mix, tilt.
    for (int o = 0; o < 3; ++o)
    {
        juce::String prefix = "osc" + juce::String (o + 1);

        oscs[o].waveformBox.addItem ("Sine", 1);
        oscs[o].waveformBox.addItem ("Square", 2);
        oscs[o].waveformBox.addItem ("Triangle", 3);
        oscs[o].waveformBox.addItem ("Sawtooth", 4);
        oscs[o].waveformBox.addItem ("Quadratic", 5);
        oscs[o].waveformBox.addItem ("Noise", 6);
        content.addAndMakeVisible (oscs[o].waveformBox);
        oscs[o].waveformLabel.setText ("Wave", juce::dontSendNotification);
        content.addAndMakeVisible (oscs[o].waveformLabel);
        oscs[o].waveformAttachment = std::make_unique<OscControl::ComboBoxAttachment> (
            processorRef.apvts, prefix + "waveform", oscs[o].waveformBox);

        oscs[o].detuneSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        oscs[o].detuneSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 16);
        content.addAndMakeVisible (oscs[o].detuneSlider);
        oscs[o].detuneLabel.setText ("Detune", juce::dontSendNotification);
        content.addAndMakeVisible (oscs[o].detuneLabel);
        oscs[o].detuneAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "detune", oscs[o].detuneSlider);

        oscs[o].mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        oscs[o].mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
        content.addAndMakeVisible (oscs[o].mixSlider);
        oscs[o].mixLabel.setText ("Mix", juce::dontSendNotification);
        oscs[o].mixLabel.setJustificationType (juce::Justification::centred);
        content.addAndMakeVisible (oscs[o].mixLabel);
        oscs[o].mixAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "mix", oscs[o].mixSlider);

        // Golden Tilt: wykładnik opadania amplitud partiali (1.0 = neutralnie).
        oscs[o].tiltSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        oscs[o].tiltSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
        oscs[o].tiltSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
            "Golden Tilt: opadanie widma a\xe2\x82\x99\xc2\xb7(n+1)^(1\xe2\x88\x92tilt); "
            "1 = naturalne, \xcf\x86\xe2\x89\x88""1.618 = z\xc5\x82ote przyciemnienie")));
        content.addAndMakeVisible (oscs[o].tiltSlider);
        oscs[o].tiltLabel.setText ("Tilt", juce::dontSendNotification);
        oscs[o].tiltLabel.setJustificationType (juce::Justification::centred);
        content.addAndMakeVisible (oscs[o].tiltLabel);
        oscs[o].tiltAttachment = std::make_unique<OscControl::SliderAttachment> (
            processorRef.apvts, prefix + "tilt", oscs[o].tiltSlider);

        oscs[o].goldIntBox.addItem (juce::CharPointer_UTF8 ("\xe2\x88\x92""2\xcf\x86"), 1);  // −2φ
        oscs[o].goldIntBox.addItem (juce::CharPointer_UTF8 ("\xe2\x88\x92\xcf\x86"), 2);      // −φ
        oscs[o].goldIntBox.addItem ("Off", 3);
        oscs[o].goldIntBox.addItem (juce::CharPointer_UTF8 ("+\xcf\x86"), 4);                 // +φ
        oscs[o].goldIntBox.addItem (juce::CharPointer_UTF8 ("+2\xcf\x86"), 5);                // +2φ
        content.addAndMakeVisible (oscs[o].goldIntBox);
        oscs[o].goldIntLabel.setText (juce::CharPointer_UTF8 ("Golden Int (\xcf\x86)"),
                                      juce::dontSendNotification);
        content.addAndMakeVisible (oscs[o].goldIntLabel);
        oscs[o].goldIntAttachment = std::make_unique<OscControl::ComboBoxAttachment> (
            processorRef.apvts, prefix + "goldint", oscs[o].goldIntBox);
    }

    // Filter
    filterCutoffSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterCutoffSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 16);
    content.addAndMakeVisible (filterCutoffSlider);
    filterCutoffLabel.setText ("Filter Cutoff", juce::dontSendNotification);
    content.addAndMakeVisible (filterCutoffLabel);
    filterCutoffAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "filterCutoff", filterCutoffSlider);

    filterResonanceSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterResonanceSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    content.addAndMakeVisible (filterResonanceSlider);
    filterResonanceLabel.setText ("Resonance", juce::dontSendNotification);
    content.addAndMakeVisible (filterResonanceLabel);
    filterResonanceAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "filterResonance", filterResonanceSlider);

    filterTypeBox.addItem ("Low-pass", 1);
    filterTypeBox.addItem ("Band-pass", 2);
    filterTypeBox.addItem ("High-pass", 3);
    content.addAndMakeVisible (filterTypeBox);
    filterTypeLabel.setText ("Type", juce::dontSendNotification);
    content.addAndMakeVisible (filterTypeLabel);
    filterTypeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "filterType", filterTypeBox);

    // LFO
    lfoRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    content.addAndMakeVisible (lfoRateSlider);
    lfoRateLabel.setText ("LFO Rate", juce::dontSendNotification);
    content.addAndMakeVisible (lfoRateLabel);
    lfoRateAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "lfoRate", lfoRateSlider);

    lfoDepthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    content.addAndMakeVisible (lfoDepthSlider);
    lfoDepthLabel.setText ("LFO Depth", juce::dontSendNotification);
    content.addAndMakeVisible (lfoDepthLabel);
    lfoDepthAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "lfoDepth", lfoDepthSlider);

    lfoShapeBox.addItem ("Sine", 1);
    lfoShapeBox.addItem ("Square", 2);
    lfoShapeBox.addItem ("Triangle", 3);
    lfoShapeBox.addItem ("Golden S&H", 4);
    content.addAndMakeVisible (lfoShapeBox);
    lfoShapeLabel.setText ("LFO Shape", juce::dontSendNotification);
    content.addAndMakeVisible (lfoShapeLabel);
    lfoShapeAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "lfoShape", lfoShapeBox);

    // Golden Drift: miks z drugim LFO o rate·φ — ruch, który się nie zapętla.
    lfoDriftSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lfoDriftSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
    lfoDriftSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Golden Drift: drugi LFO o rate\xc2\xb7\xcf\x86 \xe2\x80\x94 suma nigdy si\xc4\x99 "
        "nie powtarza (quasi-periodyczny drift)")));
    content.addAndMakeVisible (lfoDriftSlider);
    lfoDriftLabel.setText (juce::String (juce::CharPointer_UTF8 ("Drift (\xcf\x86)")),
                           juce::dontSendNotification);
    content.addAndMakeVisible (lfoDriftLabel);
    lfoDriftAttachment = std::make_unique<SliderAttachment> (
        processorRef.apvts, "lfoDrift", lfoDriftSlider);

    // LFO sync do tempa: przełącznik + podział nut (rate liczone pod tempo).
    lfoSyncButton.setButtonText ("Sync");
    lfoSyncButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Licz rate LFO z podzia\xc5\x82u nut pod tempo (wy\xc5\x82. = Hz)")));
    content.addAndMakeVisible (lfoSyncButton);
    lfoSyncAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "lfoSync", lfoSyncButton);
    lfoSyncButton.onClick = [this] { updateLfoSyncEnablement(); };

    lfoRateDivBox.addItem ("1/1", 1);
    lfoRateDivBox.addItem ("1/2", 2);
    lfoRateDivBox.addItem ("1/4", 3);
    lfoRateDivBox.addItem ("1/8", 4);
    lfoRateDivBox.addItem ("1/16", 5);
    lfoRateDivBox.addItem ("1/8T", 6);
    lfoRateDivBox.addItem ("1/16T", 7);
    content.addAndMakeVisible (lfoRateDivBox);
    lfoRateDivAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "lfoRateDiv", lfoRateDivBox);

    // === Sekcja GOLD: złoty silnik ===
    auto setupGoldSlider = [this] (juce::Slider& s, juce::Label& l, const char* text,
                                   const juce::String& paramID,
                                   std::unique_ptr<SliderAttachment>& att)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
        content.addAndMakeVisible (s);
        l.setText (juce::String (juce::CharPointer_UTF8 (text)), juce::dontSendNotification);
        content.addAndMakeVisible (l);
        att = std::make_unique<SliderAttachment> (processorRef.apvts, paramID, s);
    };

    setupGoldSlider (ringSlider, ringLabel, "Ring (osc1\xc3\x97osc2)", "ringMix", ringAttachment);
    ringSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Golden Ring: produkt osc1\xc2\xb7osc2 \xe2\x80\x94 sidebandy f1\xc2\xb1""f2; "
        "z goldint=\xcf\x86 g\xc4\x99ste, nieharmoniczne dzwony")));

    setupGoldSlider (fmSlider, fmLabel, "FM (f\xc2\xb7\xcf\x86)", "fmAmt", fmAttachment);
    fmSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Golden FM: sinus o cz\xc4\x99stotliwo\xc5\x9b""ci f\xc2\xb7\xcf\x86 moduluje "
        "wysoko\xc5\x9b\xc4\x87 wszystkich partiali \xe2\x80\x94 metaliczne, nierepetuj\xc4\x85""ce sidebandy")));

    setupGoldSlider (subSlider, subLabel, "Sub (f/\xcf\x86)", "subLevel", subAttachment);
    subSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Golden Sub: sub-partial \xe2\x88\x92""833\xc2\xa2 pod fundamentem (z\xc5\x82ota sub-oktawa)")));

    setupGoldSlider (uniSlider, uniLabel, "Unison (\xcf\x86)", "unison", uniAttachment);
    uniSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Golden Unison: bli\xc5\xbaniaki dolnych partiali rozstrojone frac(n\xc2\xb7\xcf\x86) "
        "\xe2\x80\x94 ch\xc3\xb3r bez okresowego phasingu")));

    // Golden Delay: multi-tap w proporcjach φ.
    dlyOnButton.setButtonText ("Golden Delay");
    dlyOnButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Multi-tap delay: 4 tapy w czasach t/\xcf\x86\xc2\xb3..t \xe2\x80\x94 z\xc5\x82ote "
        "odst\xc4\x99py = brak okresowego fluteru grzebieniowego")));
    content.addAndMakeVisible (dlyOnButton);
    dlyOnAttachment = std::make_unique<ButtonAttachment> (processorRef.apvts, "dlyOn", dlyOnButton);

    dlySyncButton.setButtonText ("Sync");
    content.addAndMakeVisible (dlySyncButton);
    dlySyncAttachment = std::make_unique<ButtonAttachment> (processorRef.apvts, "dlySync", dlySyncButton);
    dlySyncButton.onClick = [this] { updateDelaySyncEnablement(); };

    {
        const auto divLabels = FiSynthAudioProcessor::divisionLabels();
        for (int i = 0; i < divLabels.size(); ++i)
            dlyDivBox.addItem (divLabels[i], i + 1);
    }
    content.addAndMakeVisible (dlyDivBox);
    dlyDivAttachment = std::make_unique<ComboBoxAttachment> (processorRef.apvts, "dlyDiv", dlyDivBox);

    dlyTimeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    dlyTimeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 16);
    dlyTimeSlider.setTextValueSuffix (" ms");
    content.addAndMakeVisible (dlyTimeSlider);
    dlyTimeAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "dlyTime", dlyTimeSlider);

    dlyFbLabel.setText ("FB", juce::dontSendNotification);
    content.addAndMakeVisible (dlyFbLabel);
    dlyFbSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    dlyFbSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    content.addAndMakeVisible (dlyFbSlider);
    dlyFbAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "dlyFeedback", dlyFbSlider);

    dlyMixLabel.setText ("Mix", juce::dontSendNotification);
    content.addAndMakeVisible (dlyMixLabel);
    dlyMixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    dlyMixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
    content.addAndMakeVisible (dlyMixSlider);
    dlyMixAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "dlyMix", dlyMixSlider);

    // Kwantyzacja pitch-modu do 833.09¢ (przy narzędziach φ obwiedni).
    pitchQuantButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("833\xc2\xa2 quant")));
    pitchQuantButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Kwantyzuj modulacj\xc4\x99 Pitch do krotno\xc5\x9b""ci z\xc5\x82otego "
        "interwa\xc5\x82u 833.09\xc2\xa2 (sp\xc3\xb3jne z goldint)")));
    content.addAndMakeVisible (pitchQuantButton);
    pitchQuantAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "pitchQuant", pitchQuantButton);

    // Arpeggiator Fibonacciego (górny pasek).
    arpOnButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("Arp (\xcf\x86)")));
    arpOnButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Arpeggiator: od najni\xc5\xbcszego klawisza interwa\xc5\x82y Fibonacciego "
        "(0,1,2,3,5,8,13,21\xe2\x80\xa6 p\xc3\xb3\xc5\x82ton\xc3\xb3w)")));
    content.addAndMakeVisible (arpOnButton);
    arpOnAttachment = std::make_unique<ButtonAttachment> (processorRef.apvts, "arpOn", arpOnButton);

    {
        const auto divLabels = FiSynthAudioProcessor::divisionLabels();
        for (int i = 0; i < divLabels.size(); ++i)
            arpDivBox.addItem (divLabels[i], i + 1);
    }
    content.addAndMakeVisible (arpDivBox);
    arpDivAttachment = std::make_unique<ComboBoxAttachment> (processorRef.apvts, "arpDiv", arpDivBox);

    arpLenBox.addItem ("5", 1);
    arpLenBox.addItem ("8", 2);
    arpLenBox.addItem ("13", 3);
    content.addAndMakeVisible (arpLenBox);
    arpLenAttachment = std::make_unique<ComboBoxAttachment> (processorRef.apvts, "arpLen", arpLenBox);

    updateBpmEnablement();
    updateLfoSyncEnablement();
    updateDelaySyncEnablement();

    // Klawiatura ekranowa + wizualizery + moduły φ.
    content.addAndMakeVisible (keyboard);
    content.addAndMakeVisible (spiral);
    content.addAndMakeVisible (spectrum);
    content.addAndMakeVisible (fibGate);
    content.addAndMakeVisible (phyllo);

    aboutButton.onClick = [this]
    {
        aboutPanel.setVisible (! aboutPanel.isVisible());
        aboutPanel.toFront (false);
    };
    content.addAndMakeVisible (aboutButton);

    // Nakładka pierścieni modulacji — dodana OSTATNIA, żeby leżała nad
    // kontrolkami (mysz przez nią przechodzi).
    ModRingOverlay::Targets ringTargets;
    ringTargets.cutoff    = &filterCutoffSlider;
    ringTargets.resonance = &filterResonanceSlider;
    ringTargets.fm        = &fmSlider;
    ringTargets.spiral    = &spiral;   // ring stretcha = promień na tarczy
    for (int o = 0; o < 3; ++o)
    {
        ringTargets.mix[o]  = &oscs[o].mixSlider;
        ringTargets.tilt[o] = &oscs[o].tiltSlider;
    }
    modRings = std::make_unique<ModRingOverlay> (processorRef, ringTargets);
    content.addAndMakeVisible (*modRings);

    // About NAD wszystkim (także nad nakładką pierścieni); domyślnie schowany.
    content.addChildComponent (aboutPanel);

    // Okno resizowalne o stałym aspekcie: layout liczy się w bazie 1460×850,
    // a resized() skaluje płótno transformem do bieżącego rozmiaru.
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setResizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    setSize (baseWidth, baseHeight);
}

FiSynthAudioProcessorEditor::~FiSynthAudioProcessorEditor()
{
    content.setLookAndFeel (nullptr);
}

void FiSynthAudioProcessorEditor::applyPhiCascade()
{
    const auto phi = (float) fiPhi;
    const int env = envEditor.getActiveEnvelope();
    auto& model = processorRef.getEnvelopeModel (env);

    // Atak jak w domyślnym ADSR, potem kaskada: poziomy 1/φᵏ w segmentach,
    // z których każdy jest krótszy od poprzedniego o φ (samopodobny decay).
    model.points.clear();
    model.points.push_back ({ 0.00f, 0.0f, 0.0f });
    model.points.push_back ({ 0.05f, 1.0f, 0.0f });

    float t = 0.05f, dur = 0.45f;
    for (int k = 1; k <= 5; ++k)
    {
        t += dur;
        model.points.push_back ({ t, std::pow (phi, (float) -k), -0.35f });
        dur /= phi;
    }
    model.points.push_back ({ t + dur, 0.0f, -0.35f });

    // Sustain na końcu (poziom 0): nuta wybrzmiewa sama, jak szarpnięta struna.
    model.sustainIndex = (int) model.points.size() - 1;
    model.sortAndClamp();

    processorRef.commitEnvelope (env);
    envEditor.repaint();
}

void FiSynthAudioProcessorEditor::scaleActiveEnvelopeTimes (float factor)
{
    const int env = envEditor.getActiveEnvelope();
    auto& model = processorRef.getEnvelopeModel (env);
    if (model.points.empty())
        return;

    // 16 s to maxTime edytora obwiedni — nie wydłużamy poza edytowalny zakres.
    if (model.points.back().time * factor > 16.0f)
        return;

    for (auto& p : model.points)
        p.time *= factor;

    model.sortAndClamp();
    processorRef.commitEnvelope (env);
    envEditor.repaint();
}

void FiSynthAudioProcessorEditor::updateBpmEnablement()
{
    // Ręczne BPM ma sens tylko gdy nie synchronizujemy z DAW.
    const bool manual = ! tempoSyncButton.getToggleState();
    bpmSlider.setEnabled (manual);
    bpmLabel.setEnabled (manual);
}

void FiSynthAudioProcessorEditor::updateLfoSyncEnablement()
{
    // Sync: rate z podziału nut (combo). Bez sync: ręczny suwak w Hz.
    const bool sync = lfoSyncButton.getToggleState();
    lfoRateSlider.setEnabled (! sync);
    lfoRateDivBox.setEnabled (sync);
}

void FiSynthAudioProcessorEditor::updateDelaySyncEnablement()
{
    // Sync: czas z podziału nut (combo). Bez sync: ręczny suwak w ms.
    const bool sync = dlySyncButton.getToggleState();
    dlyTimeSlider.setEnabled (! sync);
    dlyDivBox.setEnabled (sync);
}

void FiSynthAudioProcessorEditor::selectEnvelope (int idx)
{
    envEditor.setActiveEnvelope (idx);
    envTabs[idx].setToggleState (true, juce::dontSendNotification);
}

void FiSynthAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);

    const auto list = processorRef.getPresetList();
    for (int i = 0; i < list.size(); ++i)
        presetBox.addItem (list[i], i + 1);

    const int idx = list.indexOf (processorRef.currentPresetName);
    if (idx >= 0)
        presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void FiSynthAudioProcessorEditor::loadPresetByName (const juce::String& name)
{
    const auto file = FiSynthAudioProcessor::getPresetDirectory().getChildFile (name + ".fsynth");
    if (processorRef.loadPreset (file))
    {
        // Stan parametrów podmieniony — odśwież zależne włączenia kontrolek
        // (suwaki/combo aktualizują się same przez attachmenty).
        updateBpmEnablement();
        updateLfoSyncEnablement();
        envEditor.repaint();
    }
}

void FiSynthAudioProcessorEditor::stepPreset (int delta)
{
    const auto list = processorRef.getPresetList();
    if (list.isEmpty())
        return;

    int idx = list.indexOf (processorRef.currentPresetName);
    idx = (idx < 0) ? 0 : juce::jlimit (0, list.size() - 1, idx + delta);

    const auto name = list[idx];
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
    loadPresetByName (name);
}

void FiSynthAudioProcessorEditor::newPreset()
{
    processorRef.resetToInit();

    // Parametry zresetowane (suwaki/combo aktualizują się przez attachmenty);
    // odśwież zależne włączenia i deselektuj listę presetów.
    presetBox.setSelectedId (0, juce::dontSendNotification);
    updateBpmEnablement();
    updateLfoSyncEnablement();
    envEditor.repaint();
}

void FiSynthAudioProcessorEditor::showSavePresetDialog()
{
    auto* dlg = new juce::AlertWindow ("Save Preset",
                                       "Enter a name for this preset:",
                                       juce::MessageBoxIconType::NoIcon);
    // Dialog to osobne okno desktopowe — nie dziedziczy LnF z płótna edytora,
    // więc paletę nadajemy przez ColourIds (bez wskaźnika na LnF, który mógłby
    // zwisnąć, gdy host zamknie edytor przy otwartym dialogu).
    dlg->setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff17130c));
    dlg->setColour (juce::AlertWindow::textColourId,       fiCol::text);
    dlg->setColour (juce::AlertWindow::outlineColourId,    fiCol::goldDim);
    dlg->addTextEditor ("name", processorRef.currentPresetName, "Name:");
    dlg->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // SafePointer: host może zniszczyć edytor, zanim użytkownik zamknie dialog.
    juce::Component::SafePointer<FiSynthAudioProcessorEditor> safeThis (this);
    dlg->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, dlg] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            const auto name = dlg->getTextEditorContents ("name");
            if (safeThis->processorRef.savePreset (name))
                safeThis->refreshPresetList();
        }
        delete dlg;
    }), false);
}

void FiSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Tło poza płótnem (letterbox, gdy host wymusi inny aspekt niż bazowy).
    g.fillAll (fiCol::bg);
}

void FiSynthAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (fiCol::bg);

    // Sekcja: ciepłe, niskonasycone tło + cienka złota ramka; opcjonalny
    // tytuł w lewym górnym rogu. Jedna rodzina brązów zamiast tęczy —
    // czyste złoto zostaje akcentem dla φ.
    auto section = [&g] (juce::Rectangle<int> r, juce::Colour fill,
                         const char* title = nullptr)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (fiCol::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);

        if (title != nullptr)
        {
            g.setColour (fiCol::gold.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
            g.drawText (juce::String::fromUTF8 (title),
                        r.reduced (10, 4).removeFromTop (14),
                        juce::Justification::centredLeft);
        }
    };

    static const char* oscTitles[3] = { "OSC 1", "OSC 2", "OSC 3" };
    for (int o = 0; o < 3; ++o)
        section (oscPanelBounds[o], fiCol::osc, oscTitles[o]);

    section (spiralBounds,   fiCol::viz);
    section (envBounds,      fiCol::env);
    section (filterBounds,   fiCol::filter);
    section (lfoBounds,      fiCol::lfo);
    section (goldBounds,     fiCol::goldSec, "\xcf\x86 ENGINE");
    section (gateBounds,     fiCol::gate);
    section (spectrumBounds, fiCol::viz);
    section (phylloBounds,   fiCol::stereo);

    // Logo: złote kafle (Fibonacci 8 5 3 2 1 1 + spirala) + wordmark "fibo".
    {
        auto logoRow = gainBounds.withTrimmedLeft (4).withHeight (30).toFloat();
        fiDrawLogo (g, logoRow.removeFromLeft (44.0f).reduced (0.0f, 2.5f));

        logoRow.removeFromLeft (8.0f);
        const juce::Font f (juce::FontOptions (23.0f).withStyle ("Bold"));
        g.setFont (f);
        // "o" doklejone dokładnie za zmierzoną szerokością "fib" (dwutonowy wordmark).
        const float wFib = juce::GlyphArrangement::getStringWidth (f, "fib");
        g.setColour (fiCol::text);
        g.drawText ("fib", logoRow, juce::Justification::centredLeft);
        g.setColour (fiCol::gold);
        g.drawText ("o", logoRow.withTrimmedLeft (wFib), juce::Justification::centredLeft);
    }
}

void FiSynthAudioProcessorEditor::resized()
{
    // Skala dopasowuje bazę do okna; content żyje w bazie, transform robi resztę.
    const float scale = juce::jmin (getWidth()  / (float) baseWidth,
                                    getHeight() / (float) baseHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);
    layoutContent();
}

void FiSynthAudioProcessorEditor::layoutContent()
{
    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (5);

    // === KLAWIATURA: pełna szerokość na dole ===
    {
        auto kbArea = area.removeFromBottom (64);
        kbArea.removeFromTop (6);
        keyboard.setKeyWidth ((float) kbArea.getWidth() / 36.0f);  // 36 białych klawiszy C2..C7
        keyboard.setBounds (kbArea);
    }

    // === GÓRNY PASEK: logo Fibo + tempo + Volume ===
    gainBounds = area.removeFromTop (90);
    auto topBar = gainBounds;

    // Volume po prawej, żeby nie zasłaniać logo.
    auto volCol = topBar.removeFromRight (110);
    gainLabel.setBounds (volCol.removeFromTop (16));
    gainSlider.setBounds (volCol.withSizeKeepingCentre (84, 74));

    // Lewa strona: logo na górze (rysowane w paint), kontrolki tempa pod nim.
    // Pasek presetów leży w tym samym strip co logo, po prawej od napisu "Fibo".
    {
        auto logoStrip = topBar.removeFromTop (32).reduced (0, 3);
        logoStrip.removeFromLeft (116);  // miejsce na logo (kafle + "fibo")
        presetNewButton.setBounds (logoStrip.removeFromLeft (48).reduced (1, 0));
        logoStrip.removeFromLeft (8);
        presetPrevButton.setBounds (logoStrip.removeFromLeft (26).reduced (1, 0));
        presetBox.setBounds       (logoStrip.removeFromLeft (200).reduced (3, 0));
        presetNextButton.setBounds (logoStrip.removeFromLeft (26).reduced (1, 0));
        logoStrip.removeFromLeft (10);
        presetSaveButton.setBounds (logoStrip.removeFromLeft (66).reduced (2, 0));
        logoStrip.removeFromLeft (10);
        aboutButton.setBounds (logoStrip.removeFromLeft (64).reduced (2, 0));
    }
    auto tempoRow = topBar.removeFromTop (26);
    tempoSyncButton.setBounds (tempoRow.removeFromLeft (110).reduced (4, 0));
    envSyncButton.setBounds (tempoRow.removeFromLeft (60).reduced (4, 0));
    envSnapButton.setBounds (tempoRow.removeFromLeft (60).reduced (4, 0));
    envGridBox.setBounds (tempoRow.removeFromLeft (64).reduced (4, 1));
    bpmLabel.setBounds (tempoRow.removeFromLeft (38).reduced (2, 0));
    bpmSlider.setBounds (tempoRow.removeFromLeft (110).reduced (2, 1));

    // Arpeggiator Fibonacciego — w wierszu tempa (to funkcja "performance").
    tempoRow.removeFromLeft (14);
    arpOnButton.setBounds (tempoRow.removeFromLeft (76).reduced (2, 0));
    arpDivBox.setBounds (tempoRow.removeFromLeft (64).reduced (2, 1));
    arpLenBox.setBounds (tempoRow.removeFromLeft (52).reduced (2, 1));
    area.removeFromTop (8);  // margin

    // === DOLNY PAS: gate Fibonacciego | widmo | pole stereo ===
    {
        auto bottom = area.removeFromBottom (206);
        gateBounds = bottom.removeFromLeft (330);
        bottom.removeFromLeft (8);
        phylloBounds = bottom.removeFromRight (330);
        bottom.removeFromRight (8);
        spectrumBounds = bottom;

        fibGate.setBounds (gateBounds.reduced (5));
        spectrum.setBounds (spectrumBounds.reduced (5));
        phyllo.setBounds (phylloBounds.reduced (5));
        area.removeFromBottom (8);
    }

    // === GŁÓWNY RZĄD: oscylatory | spirala-kontroler | modulacja ===
    // (układ "hero": spirala w centrum jako wizualizer i pad stretch×tryb)

    // LEWA kolumna: 3 panele oscylatorów.
    {
        auto oscCol = area.removeFromLeft (380);
        const int panelH = (oscCol.getHeight() - 2 * 8) / 3;

        for (int o = 0; o < 3; ++o)
        {
            oscPanelBounds[o] = oscCol.removeFromTop (panelH);
            if (o < 2)
                oscCol.removeFromTop (8);

            auto p = oscPanelBounds[o].reduced (8);
            p.removeFromTop (14);   // malowany tytuł "OSC n"

            auto labels = p.removeFromTop (14);
            auto combos = p.removeFromTop (22);
            auto goldIntL = labels.removeFromRight (120);
            auto goldIntC = combos.removeFromRight (120);
            oscs[o].waveformLabel.setBounds (labels.reduced (3, 0));
            oscs[o].waveformBox.setBounds (combos.withTrimmedRight (6).reduced (3, 0));
            oscs[o].goldIntLabel.setBounds (goldIntL.reduced (3, 0));
            oscs[o].goldIntBox.setBounds (goldIntC.reduced (3, 0));

            p.removeFromTop (4);
            auto detRow = p.removeFromTop (22);
            oscs[o].detuneLabel.setBounds (detRow.removeFromLeft (52));
            oscs[o].detuneSlider.setBounds (detRow.reduced (3, 0));

            p.removeFromTop (2);
            auto mixCol  = p.removeFromLeft (p.getWidth() / 2);
            oscs[o].mixLabel.setBounds (mixCol.removeFromTop (13));
            oscs[o].mixSlider.setBounds (mixCol.withSizeKeepingCentre (66, juce::jmin (72, mixCol.getHeight())));
            oscs[o].tiltLabel.setBounds (p.removeFromTop (13));
            oscs[o].tiltSlider.setBounds (p.withSizeKeepingCentre (66, juce::jmin (72, p.getHeight())));
        }
        area.removeFromLeft (8);
    }

    // CENTRUM: spirala φ (wizualizer + kontroler stretch/tryb).
    spiralBounds = area.removeFromLeft (500);
    spiral.setBounds (spiralBounds.reduced (4));
    area.removeFromLeft (8);

    // PRAWA kolumna: obwiednie + routing, filtr, LFO, silnik φ.
    // === ENVELOPE: graficzny edytor + routing modulacji ===
    envBounds = area.removeFromTop (300);
    auto envArea = envBounds.reduced (5);

    auto envHeader = envArea.removeFromTop (22);
    envTitleLabel.setBounds (envHeader.removeFromLeft (60).withSizeKeepingCentre (60, 16));
    for (int e = 0; e < kNumEnvelopes; ++e)
        envTabs[e].setBounds (envHeader.removeFromLeft (40).reduced (2, 0));

    // Narzędzia φ aktywnej obwiedni, za zakładkami.
    envHeader.removeFromLeft (8);
    phiCascadeButton.setBounds (envHeader.removeFromLeft (60).reduced (2, 0));
    phiMulButton.setBounds (envHeader.removeFromLeft (38).reduced (2, 0));
    phiDivButton.setBounds (envHeader.removeFromLeft (38).reduced (2, 0));
    envHeader.removeFromLeft (8);
    pitchQuantButton.setBounds (envHeader.removeFromLeft (104).reduced (2, 0));

    // 3 wiersze routingu na dole sekcji
    auto routing = envArea.removeFromBottom (3 * 26 + 4);
    for (int slot = 0; slot < 3; ++slot)
    {
        auto row = routing.removeFromTop (26).reduced (0, 1);
        modSlots[slot].label.setBounds (row.removeFromLeft (58));
        modSlots[slot].destBox.setBounds (row.removeFromLeft (124).reduced (2, 1));
        modSlots[slot].amtSlider.setBounds (row.reduced (4, 1));
    }

    envArea.removeFromBottom (6);
    envEditor.setBounds (envArea);
    area.removeFromTop (8);

    // === FILTER ===
    filterBounds = area.removeFromTop (46);
    auto filterArea = filterBounds.reduced (5);
    auto filterLabels = filterArea.removeFromTop (14);
    filterCutoffLabel.setBounds (filterLabels.removeFromLeft (200));
    filterResonanceLabel.setBounds (filterLabels.removeFromLeft (160));
    filterTypeLabel.setBounds (filterLabels.removeFromLeft (130));

    auto filterSliders = filterArea.removeFromTop (20);
    filterCutoffSlider.setBounds (filterSliders.removeFromLeft (200).reduced (3, 0));
    filterResonanceSlider.setBounds (filterSliders.removeFromLeft (160).reduced (3, 0));
    filterTypeBox.setBounds (filterSliders.removeFromLeft (130).reduced (3, 0));
    area.removeFromTop (8);

    // === LFO ===
    lfoBounds = area.removeFromTop (80);
    auto lfoArea = lfoBounds.reduced (5);
    auto lfoLabels = lfoArea.removeFromTop (14);
    lfoRateLabel.setBounds (lfoLabels.removeFromLeft (135));
    lfoDepthLabel.setBounds (lfoLabels.removeFromLeft (135));
    lfoShapeLabel.setBounds (lfoLabels.removeFromLeft (115));
    lfoDriftLabel.setBounds (lfoLabels.removeFromLeft (140));

    auto lfoSliders = lfoArea.removeFromTop (20);
    lfoRateSlider.setBounds (lfoSliders.removeFromLeft (135).reduced (3, 0));
    lfoDepthSlider.setBounds (lfoSliders.removeFromLeft (135).reduced (3, 0));
    lfoShapeBox.setBounds (lfoSliders.removeFromLeft (115).reduced (3, 1));
    lfoDriftSlider.setBounds (lfoSliders.removeFromLeft (140).reduced (3, 0));

    // Druga linia pod kolumną Rate: przełącznik sync + podział nut.
    lfoArea.removeFromTop (4);
    auto lfoSyncRow = lfoArea.removeFromTop (20);
    lfoSyncButton.setBounds (lfoSyncRow.removeFromLeft (70).reduced (3, 0));
    lfoRateDivBox.setBounds (lfoSyncRow.removeFromLeft (76).reduced (3, 0));
    area.removeFromTop (8);

    // === φ ENGINE: ring / fm / sub / unison + Golden Delay ===
    goldBounds = area;
    auto goldArea = goldBounds.reduced (5);
    goldArea.removeFromTop (14);   // malowany tytuł "φ ENGINE"

    auto goldRow1 = goldArea.removeFromTop (20);
    ringLabel.setBounds (goldRow1.removeFromLeft (118));
    ringSlider.setBounds (goldRow1.removeFromLeft (145).reduced (2, 0));
    goldRow1.removeFromLeft (8);
    fmLabel.setBounds (goldRow1.removeFromLeft (85));
    fmSlider.setBounds (goldRow1.reduced (2, 0));

    goldArea.removeFromTop (3);
    auto goldRow2 = goldArea.removeFromTop (20);
    subLabel.setBounds (goldRow2.removeFromLeft (118));
    subSlider.setBounds (goldRow2.removeFromLeft (145).reduced (2, 0));
    goldRow2.removeFromLeft (8);
    uniLabel.setBounds (goldRow2.removeFromLeft (85));
    uniSlider.setBounds (goldRow2.reduced (2, 0));

    goldArea.removeFromTop (3);
    auto goldRow3 = goldArea.removeFromTop (22);
    dlyOnButton.setBounds (goldRow3.removeFromLeft (104).reduced (1, 0));
    dlySyncButton.setBounds (goldRow3.removeFromLeft (52).reduced (1, 0));
    dlyDivBox.setBounds (goldRow3.removeFromLeft (58).reduced (2, 1));
    dlyTimeSlider.setBounds (goldRow3.removeFromLeft (110).reduced (2, 0));
    goldRow3.removeFromLeft (4);
    dlyFbLabel.setBounds (goldRow3.removeFromLeft (28));
    dlyFbSlider.setBounds (goldRow3.removeFromLeft (80).reduced (2, 0));
    goldRow3.removeFromLeft (4);
    dlyMixLabel.setBounds (goldRow3.removeFromLeft (34));
    dlyMixSlider.setBounds (goldRow3.reduced (2, 0));

    // Nakładki nad całym płótnem: pierścienie modulacji i panel About.
    const juce::Rectangle<int> base (0, 0, baseWidth, baseHeight);
    if (modRings != nullptr)
        modRings->setBounds (base);
    aboutPanel.setBounds (base);
}
