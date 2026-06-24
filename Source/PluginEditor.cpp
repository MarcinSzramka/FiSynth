#include "PluginEditor.h"

FiSynthAudioProcessorEditor::FiSynthAudioProcessorEditor (FiSynthAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), envEditor (p)
{
    // Gain + Stretch
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (gainSlider);
    gainLabel.setText ("Volume", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (gainLabel);
    gainAttachment = std::make_unique<SliderAttachment> (processorRef.apvts, "gain", gainSlider);

    // Źródło tempa: wł. = synchronizacja z tempem DAW, wył. = ręczne BPM poniżej.
    tempoSyncButton.setButtonText ("Sync Tempo");
    tempoSyncButton.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    tempoSyncButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Synchronizuj z tempem DAW (wy\xc5\x82. = u\xc5\xbcyj r\xc4\x99cznego BPM)")));
    addAndMakeVisible (tempoSyncButton);
    tempoSyncAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "tempoSync", tempoSyncButton);
    // Gdy synchronizujemy z DAW, ręczny suwak BPM jest nieaktywny.
    tempoSyncButton.onClick = [this] { updateBpmEnablement(); };

    // Graficzna obwiednia
    addAndMakeVisible (envEditor);
    envTitleLabel.setText ("Envelope", juce::dontSendNotification);
    envTitleLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (envTitleLabel);

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
        addAndMakeVisible (envTabs[e]);
    }

    // === Pasek presetów ===
    presetBox.setTextWhenNothingSelected (juce::String (juce::CharPointer_UTF8 ("Preset\xe2\x80\xa6")));
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff202833));
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty())
            loadPresetByName (name);
    };
    addAndMakeVisible (presetBox);

    presetNewButton.onClick  = [this] { newPreset(); };
    presetPrevButton.onClick = [this] { stepPreset (-1); };
    presetNextButton.onClick = [this] { stepPreset (+1); };
    presetSaveButton.onClick = [this] { showSavePresetDialog(); };
    addAndMakeVisible (presetNewButton);
    addAndMakeVisible (presetPrevButton);
    addAndMakeVisible (presetNextButton);
    addAndMakeVisible (presetSaveButton);

    refreshPresetList();

    // Sync do tempa + siatka. Czasy obwiedni stają się beatami i podążają za BPM.
    envSyncButton.setButtonText ("Sync");
    envSyncButton.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (envSyncButton);
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

    envGridBox.addItem ("1/4", 1);
    envGridBox.addItem ("1/8", 2);
    envGridBox.addItem ("1/16", 3);
    envGridBox.addItem ("1/32", 4);
    envGridBox.addItem ("1/8T", 5);
    envGridBox.addItem ("1/16T", 6);
    addAndMakeVisible (envGridBox);
    envGridAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "envGrid", envGridBox);

    envSnapButton.setButtonText ("Snap");
    envSnapButton.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible (envSnapButton);
    envSnapAttachment = std::make_unique<ButtonAttachment> (
        processorRef.apvts, "envSnap", envSnapButton);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 16);
    bpmSlider.setNumDecimalPlacesToDisplay (1);
    bpmSlider.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "R\xc4\x99""czne BPM (u\xc5\xbcywane gdy host/DAW nie podaje tempa)")));
    addAndMakeVisible (bpmSlider);
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
        addAndMakeVisible (modSlots[slot].destBox);

        modSlots[slot].amtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        modSlots[slot].amtSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 16);
        addAndMakeVisible (modSlots[slot].amtSlider);

        // Slot N jest napędzany obwiednią modulacyjną o tym samym numerze.
        modSlots[slot].label.setText ("Env " + juce::String (slot + 1)
                                          + juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x92")),
                                      juce::dontSendNotification);
        modSlots[slot].label.setColour (juce::Label::textColourId,
                                        EnvelopeEditor::envelopeColour (slot + 1));
        addAndMakeVisible (modSlots[slot].label);

        modSlots[slot].destAttachment = std::make_unique<ComboBoxAttachment> (
            processorRef.apvts, prefix + "Dest", modSlots[slot].destBox);
        modSlots[slot].amtAttachment = std::make_unique<SliderAttachment> (
            processorRef.apvts, prefix + "Amt", modSlots[slot].amtSlider);
    }

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

    // LFO sync do tempa: przełącznik + podział nut (rate liczone pod tempo).
    lfoSyncButton.setButtonText ("Sync");
    lfoSyncButton.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    lfoSyncButton.setTooltip (juce::String (juce::CharPointer_UTF8 (
        "Licz rate LFO z podzia\xc5\x82u nut pod tempo (wy\xc5\x82. = Hz)")));
    addAndMakeVisible (lfoSyncButton);
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
    addAndMakeVisible (lfoRateDivBox);
    lfoRateDivAttachment = std::make_unique<ComboBoxAttachment> (
        processorRef.apvts, "lfoRateDiv", lfoRateDivBox);

    updateBpmEnablement();
    updateLfoSyncEnablement();

    setSize (900, 780);
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
    dlg->addTextEditor ("name", processorRef.currentPresetName, "Name:");
    dlg->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    dlg->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    dlg->enterModalState (true, juce::ModalCallbackFunction::create ([this, dlg] (int result)
    {
        if (result == 1)
        {
            const auto name = dlg->getTextEditorContents ("name");
            if (processorRef.savePreset (name))
                refreshPresetList();
        }
        delete dlg;
    }), false);
}

void FiSynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0a));

    // Kolorowe sekcje
    g.setColour (juce::Colour (0xff1a4d7a));  // ciemny niebieski - OSC
    g.fillRect (oscBounds);

    g.setColour (juce::Colour (0xff1a5c2a));  // ciemny zielony - Envelope
    g.fillRect (envBounds);

    g.setColour (juce::Colour (0xff664d0a));  // ciemny żółty - Filter
    g.fillRect (filterBounds);

    g.setColour (juce::Colour (0xff7a1a1a));  // ciemny czerwony - LFO
    g.fillRect (lfoBounds);

    // Logo
    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    g.drawText ("Fibo", gainBounds.withTrimmedLeft (4).withHeight (30),
                juce::Justification::left);
}

void FiSynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (5);

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
        logoStrip.removeFromLeft (74);  // miejsce na logo "Fibo"
        presetNewButton.setBounds (logoStrip.removeFromLeft (48).reduced (1, 0));
        logoStrip.removeFromLeft (8);
        presetPrevButton.setBounds (logoStrip.removeFromLeft (26).reduced (1, 0));
        presetBox.setBounds       (logoStrip.removeFromLeft (200).reduced (3, 0));
        presetNextButton.setBounds (logoStrip.removeFromLeft (26).reduced (1, 0));
        logoStrip.removeFromLeft (10);
        presetSaveButton.setBounds (logoStrip.removeFromLeft (66).reduced (2, 0));
    }
    auto tempoRow = topBar.removeFromTop (26);
    tempoSyncButton.setBounds (tempoRow.removeFromLeft (110).reduced (4, 0));
    envSyncButton.setBounds (tempoRow.removeFromLeft (60).reduced (4, 0));
    envSnapButton.setBounds (tempoRow.removeFromLeft (60).reduced (4, 0));
    envGridBox.setBounds (tempoRow.removeFromLeft (64).reduced (4, 1));
    bpmLabel.setBounds (tempoRow.removeFromLeft (38).reduced (2, 0));
    bpmSlider.setBounds (tempoRow.removeFromLeft (110).reduced (2, 1));
    area.removeFromTop (8);  // margin

    // === OSCILLATORY (niebieski) ===
    oscBounds = area.removeFromTop (250);
    auto oscArea = oscBounds.reduced (5);
    for (int o = 0; o < 3; ++o)
    {
        auto row = oscArea.removeFromTop (78);

        // Waveform + Detune na górze
        auto waveCol = row.removeFromLeft (160);
        oscs[o].waveformLabel.setBounds (waveCol.removeFromTop (16));
        oscs[o].waveformBox.setBounds (waveCol.removeFromTop (24).reduced (3, 0));

        oscs[o].detuneLabel.setBounds (row.removeFromTop (16));
        oscs[o].detuneSlider.setBounds (row.removeFromLeft (180).removeFromTop (24).reduced (3, 0));

        // Mix i Stretch na dole (równo obok siebie)
        auto mixCol = row.removeFromLeft (row.getWidth() / 2);
        oscs[o].mixLabel.setBounds (mixCol.removeFromTop (14));
        oscs[o].mixSlider.setBounds (mixCol.withSizeKeepingCentre (80, 80));

        oscs[o].stretchLabel.setBounds (row.removeFromTop (14));
        oscs[o].stretchSlider.setBounds (row.withSizeKeepingCentre (80, 80));
    }
    area.removeFromTop (8);  // margin

    // === ENVELOPE (zielony): graficzny edytor + routing modulacji ===
    envBounds = area.removeFromTop (230);
    auto envArea = envBounds.reduced (5);

    auto envHeader = envArea.removeFromTop (22);
    envTitleLabel.setBounds (envHeader.removeFromLeft (64).withSizeKeepingCentre (64, 16));
    for (int e = 0; e < kNumEnvelopes; ++e)
        envTabs[e].setBounds (envHeader.removeFromLeft (42).reduced (2, 0));

    // 3 wiersze routingu na dole sekcji
    auto routing = envArea.removeFromBottom (3 * 26 + 4);
    for (int slot = 0; slot < 3; ++slot)
    {
        auto row = routing.removeFromTop (26).reduced (0, 1);
        modSlots[slot].label.setBounds (row.removeFromLeft (60));
        modSlots[slot].destBox.setBounds (row.removeFromLeft (130).reduced (2, 1));
        modSlots[slot].amtSlider.setBounds (row.reduced (4, 1));
    }

    envArea.removeFromBottom (6);
    envEditor.setBounds (envArea);
    area.removeFromTop (8);

    // === FILTER (żółty) ===
    filterBounds = area.removeFromTop (50);
    auto filterArea = filterBounds.reduced (5);
    auto filterLabels = filterArea.removeFromTop (16);
    filterCutoffLabel.setBounds (filterLabels.removeFromLeft (150));
    filterResonanceLabel.setBounds (filterLabels.removeFromLeft (120));
    filterTypeLabel.setBounds (filterLabels.removeFromLeft (100));

    auto filterSliders = filterArea.removeFromTop (24);
    filterCutoffSlider.setBounds (filterSliders.removeFromLeft (150).reduced (3, 0));
    filterResonanceSlider.setBounds (filterSliders.removeFromLeft (120).reduced (3, 0));
    filterTypeBox.setBounds (filterSliders.removeFromLeft (100).reduced (3, 0));
    area.removeFromTop (8);

    // === LFO (czerwony) ===
    lfoBounds = area.removeFromTop (80);
    auto lfoArea = lfoBounds.reduced (5);
    auto lfoLabels = lfoArea.removeFromTop (16);
    lfoRateLabel.setBounds (lfoLabels.removeFromLeft (150));
    lfoDepthLabel.setBounds (lfoLabels.removeFromLeft (150));
    lfoShapeLabel.setBounds (lfoLabels.removeFromLeft (100));

    auto lfoSliders = lfoArea.removeFromTop (24);
    lfoRateSlider.setBounds (lfoSliders.removeFromLeft (150).reduced (3, 0));
    lfoDepthSlider.setBounds (lfoSliders.removeFromLeft (150).reduced (3, 0));
    lfoShapeBox.setBounds (lfoSliders.removeFromLeft (100).reduced (3, 0));

    // Druga linia pod kolumną Rate: przełącznik sync + podział nut.
    auto lfoSyncRow = lfoArea.removeFromTop (24);
    lfoSyncButton.setBounds (lfoSyncRow.removeFromLeft (70).reduced (3, 0));
    lfoRateDivBox.setBounds (lfoSyncRow.removeFromLeft (76).reduced (3, 1));
}
