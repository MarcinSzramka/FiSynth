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
#include "MidiLearn.h"

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
                         phylloBounds, spectrumBounds, fxBounds;

    // Slidery parametrów jako LearnSlider (prawy klik = menu MIDI Learn);
    // cele rejestruje initMidiLearn() na końcu konstruktora.
    LearnSlider gainSlider;
    juce::Label gainLabel;
    void initMidiLearn();

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

#if FISYNTH_DEMO
    // Plakietka DEMO w górnym pasku: dyskretna ramka, a w czasie przerwy
    // dźwięku pełne złoto z odliczaniem (odpytuje atomik procesora timerem).
    struct DemoBadge : juce::Component, juce::Timer
    {
        explicit DemoBadge (FiSynthAudioProcessor& p) : proc (p)
        {
            setInterceptsMouseClicks (false, false);
            startTimerHz (10);
        }

        void timerCallback() override
        {
            const float s = proc.demoMuteSecondsLeft.load (std::memory_order_relaxed);
            if (s != secondsLeft)
            {
                secondsLeft = s;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            if (secondsLeft >= 0.0f)
            {
                g.setColour (fiCol::gold);
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (fiCol::bg);
                g.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
                g.drawText (juce::String (juce::CharPointer_UTF8 (
                                "DEMO \xe2\x80\xa2 d\xc5\xba""wi\xc4\x99k za "))
                                + juce::String (secondsLeft, 1) + " s",
                            getLocalBounds(), juce::Justification::centred);
            }
            else
            {
                g.setColour (fiCol::goldDim);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
                g.setColour (fiCol::textDim);
                g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
                g.drawText ("DEMO", getLocalBounds(), juce::Justification::centred);
            }
        }

        FiSynthAudioProcessor& proc;
        float secondsLeft { -1.0f };
    };
    DemoBadge demoBadge;

    // Nakładka na całe płótno w czasie przerwy: sama plakietka w pasku ginęła
    // wśród kontrolek i użytkownik brał ciszę za awarię. Przyciemnia całe UI
    // i wykłada wprost, że to limit wersji demo, z odliczaniem do powrotu.
    // Nie łapie myszy — pod spodem wszystko dalej działa (można kręcić dalej).
    struct DemoMuteOverlay : juce::Component, juce::Timer
    {
        explicit DemoMuteOverlay (FiSynthAudioProcessor& p) : proc (p)
        {
            setInterceptsMouseClicks (false, false);
            startTimerHz (10);
        }

        void timerCallback() override
        {
            const float s = proc.demoMuteSecondsLeft.load (std::memory_order_relaxed);
            if (s == secondsLeft)
                return;

            const bool wasMuted = secondsLeft >= 0.0f;
            secondsLeft = s;
            const bool isMuted = secondsLeft >= 0.0f;

            if (isMuted != wasMuted)
                setVisible (isMuted);
            if (isMuted)
                repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (secondsLeft < 0.0f)
                return;

            // Przyciemnienie całego UI — czytelny sygnał "teraz nie gra".
            g.fillAll (fiCol::bg.withAlpha (0.72f));

            auto panel = getLocalBounds().withSizeKeepingCentre (560, 200).toFloat();
            g.setColour (fiCol::bg);
            g.fillRoundedRectangle (panel, 10.0f);
            g.setColour (fiCol::gold);
            g.drawRoundedRectangle (panel, 10.0f, 2.0f);

            auto r = panel.toNearestInt().reduced (24, 20);

            g.setColour (fiCol::gold);
            g.setFont (juce::Font (juce::FontOptions (34.0f).withStyle ("Bold")));
            g.drawText ("WERSJA DEMO", r.removeFromTop (42),
                        juce::Justification::centred);

            r.removeFromTop (6);
            g.setColour (fiCol::text);
            g.setFont (juce::Font (juce::FontOptions (17.0f)));
            g.drawText (juce::String (juce::CharPointer_UTF8 (
                            "D\xc5\xba""wi\xc4\x99k jest wyciszony co 20 sekund")),
                        r.removeFromTop (26), juce::Justification::centred);

            r.removeFromTop (10);
            g.setColour (fiCol::gold);
            g.setFont (juce::Font (juce::FontOptions (30.0f).withStyle ("Bold")));
            g.drawText (juce::String (secondsLeft, 1) + " s",
                        r.removeFromTop (38), juce::Justification::centred);

            g.setColour (fiCol::textDim);
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawText (juce::String (juce::CharPointer_UTF8 (
                            "do powrotu d\xc5\xba""wi\xc4\x99ku \xe2\x80\xa2 pe\xc5\x82na wersja gra bez przerw")),
                        r.removeFromTop (20), juce::Justification::centred);
        }

        FiSynthAudioProcessor& proc;
        float secondsLeft { -1.0f };
    };
    DemoMuteOverlay demoMuteOverlay;
#endif

    // === Pasek presetów (zapis/wczytywanie pełnego brzmienia) ===
    juce::ComboBox   presetBox;
    juce::TextButton presetNewButton  { "New" };
    juce::TextButton presetPrevButton { "<" }, presetNextButton { ">" }, presetSaveButton { "Save" };
    void refreshPresetList();
    void loadPresetByName (const juce::String& name);
    void stepPreset (int delta);
    void newPreset();
    void showSavePresetDialog();

    // === Mapy MIDI (przycisk MIDI → menu zapisu/wczytywania przypisań CC) ===
    juce::TextButton midiMapButton { "MIDI" };
    void showMidiMapMenu();
    void showSaveMidiMapDialog();
    void importMidiMapDialog();
    void exportMidiMapDialog();

    // Dialog importu/eksportu map — członek: musi przeżyć czas async wyboru.
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Kropka aktywności MIDI: miga, gdy z zewnątrz (sprzęt/DAW) przychodzą
    // eventy kanałowe — pierwsza diagnoza "czy MIDI w ogóle dochodzi".
    struct MidiActivityLight : juce::Component, private juce::Timer
    {
        explicit MidiActivityLight (FiSynthAudioProcessor& p)
            : proc (p), lastCount (p.midiEventCount.load (std::memory_order_relaxed))
        {
            setInterceptsMouseClicks (false, false);
            startTimerHz (20);
        }

        void timerCallback() override
        {
            const auto n = proc.midiEventCount.load (std::memory_order_relaxed);
            if (n != lastCount) { lastCount = n; hold = 3; }   // ~150 ms świecenia
            const bool on = hold > 0;
            if (hold > 0) --hold;
            if (on != lit) { lit = on; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            const auto c = getLocalBounds().toFloat().getCentre();
            const juce::Rectangle<float> dot (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
            if (lit) { g.setColour (fiCol::gold);    g.fillEllipse (dot); }
            else     { g.setColour (fiCol::goldDim); g.drawEllipse (dot.reduced (0.5f), 1.0f); }
        }

        FiSynthAudioProcessor& proc;
        juce::uint32 lastCount;
        int  hold { 0 };
        bool lit  { false };
    };
    MidiActivityLight midiLight { processorRef };

    // Synchronizacja obwiedni do tempa: sync, podział siatki, snap, ręczne BPM.
    // tempoSyncButton wybiera źródło tempa: DAW (wł.) vs ręczne BPM (wył.).
    juce::ToggleButton tempoSyncButton, envSyncButton, envSnapButton;
    juce::ComboBox     envGridBox;
    LearnSlider        bpmSlider;
    juce::Label        bpmLabel;
    void updateBpmEnablement();

    struct ModSlotControl
    {
        juce::ComboBox destBox;
        LearnSlider    amtSlider;
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
        LearnSlider detuneSlider;
        LearnSlider mixSlider;
        LearnSlider tiltSlider;
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
    LearnSlider filterCutoffSlider, filterResonanceSlider;
    juce::ComboBox filterTypeBox;
    juce::Label filterCutoffLabel, filterResonanceLabel, filterTypeLabel;

    // LFO
    LearnSlider lfoRateSlider, lfoDepthSlider, lfoDriftSlider;
    juce::ComboBox lfoShapeBox;
    juce::Label lfoRateLabel, lfoDepthLabel, lfoShapeLabel, lfoDriftLabel;
    juce::ToggleButton lfoSyncButton;   // wł. = rate z podziału nut pod tempo
    juce::ComboBox     lfoRateDivBox;
    void updateLfoSyncEnablement();

    // === Sekcja GOLD: złoty silnik (Ring/FM/Sub/Unison) + Golden Delay ===
    LearnSlider  ringSlider, fmSlider, subSlider, uniSlider;
    juce::Label  ringLabel, fmLabel, subLabel, uniLabel;
    juce::ToggleButton dlyOnButton, dlySyncButton;
    juce::ComboBox     dlyDivBox;
    LearnSlider        dlyTimeSlider, dlyFbSlider, dlyMixSlider;
    juce::Label        dlyFbLabel, dlyMixLabel;
    void updateDelaySyncEnablement();

    // Arpeggiator Fibonacciego (górny pasek, obok kontrolek tempa).
    juce::ToggleButton arpOnButton, arpWordButton;
    juce::ComboBox     arpDivBox, arpLenBox, arpModeBox;
    LearnSlider        arpVelSlider;
    juce::Label        arpVelLabel;

    // === Efektor (dolny pas, między Gate a widmem) ===
    juce::ToggleButton fxOnButton;
    LearnSlider        fxDistSlider, fxSatSlider, fxShapeSlider,
                       fxRevSizeSlider, fxRevMixSlider;
    juce::Label        fxDistLabel, fxSatLabel, fxShapeLabel,
                       fxRevSizeLabel, fxRevMixLabel;

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
    std::unique_ptr<ButtonAttachment>   fxOnAttachment;
    std::unique_ptr<SliderAttachment>   fxDistAttachment, fxSatAttachment, fxShapeAttachment,
                                        fxRevSizeAttachment, fxRevMixAttachment;

    // Nakładka pierścieni modulacji trzyma surowe wskaźniki do sliderów-celów,
    // więc jest zadeklarowana OSTATNIA — niszczona PRZED sliderami (odwrotna
    // kolejność destrukcji), nigdy nie przeżywa swoich celów.
    std::unique_ptr<ModRingOverlay> modRings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiSynthAudioProcessorEditor)
};
