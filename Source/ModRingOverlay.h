#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpiralVisualizer;

// === Pierścienie modulacji na kontrolkach-celach (styl Vital) ===
//
// Przezroczysta nakładka nad całym edytorem (mysz przechodzi przez nią).
// Dla każdego slotu env→dest z niezerowym amount rysuje przy kontrolce-celu:
//  - łuk (gałka) / kreskę (suwak) zakresu modulacji w kolorze obwiedni-źródła,
//  - kropkę na żywo dla każdego grającego głosu — wartość obwiedni w czasie
//    playheada, liczoną z krzywej (fiEnvEvalRange), jak kropka w EnvelopeEditor.
// Wartości docelowe liczone wzorami z ModMath.h — tymi samymi, których używa
// SynthVoice::renderNextBlock, więc pierścień pokazuje realny zakres ruchu.
// Ring pokazuje wkład SLOTU obwiedni; LFO (osobny modulator bazowego cutoffa)
// celowo nie wchodzi w rysowany zakres.
class ModRingOverlay : public juce::Component, private juce::Timer
{
public:
    struct Targets
    {
        juce::Slider* cutoff     = nullptr;   // linear — kreska pod suwakiem
        juce::Slider* resonance  = nullptr;
        juce::Slider* fm         = nullptr;   // linear — suwak Golden FM
        juce::Slider* mix[3]     = {};        // rotary — łuk wokół gałki
        juce::Slider* tilt[3]    = {};        // rotary — gałki Golden Tilt

        // Stretch nie ma już gałek — jest sterowany na spirali, więc jego ring
        // to promień na tarczy (geometria z SpiralVisualizer::stretchPointFor).
        SpiralVisualizer* spiral = nullptr;
    };

    ModRingOverlay (FiSynthAudioProcessor& p, const Targets& t);
    ~ModRingOverlay() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    // Wpisuje do out wartości obwiedni (0..1) grających głosów źródła envIdx
    // (1..3); zwraca ich liczbę.
    int voiceEnvValues (int envIdx, float* out) const;

    // Obszary kontrolek z aktywnym pierścieniem (do celowanego repaint).
    void collectDirtyRects (juce::RectangleList<int>& rects) const;

    FiSynthAudioProcessor& processor;
    Targets targets;

    std::atomic<float>* destPar[3] {};
    std::atomic<float>* amtPar[3] {};

    // Baza i tryb stretcha per osc — do rysowania ringu na spirali (tryb
    // zmienia kąt promienia, więc wchodzi też w sygnaturę odświeżania).
    std::atomic<float>* stretchRaw[3] {};
    std::atomic<float>* modeRaw[3] {};

    // Migawki obwiedni 1..3 do ewaluacji krzywej (odświeżane po edycji).
    EnvSnapshot snaps[3];
    int lastEditCount { -1 };

    // Sygnatura stanu z poprzedniego ticku: routing (3+3) + playheady (3·sloty)
    // + wartości celów: cutoff, rezonans, fm + 3·(mix, tilt) + 3·(stretch, tryb)
    // (15) + licznik edycji obwiedni (1).
    static constexpr int sigLen = 6 + 3 * FiSynthAudioProcessor::kMaxPlayheads + 15 + 1;
    float lastSig[sigLen] {};
    juce::RectangleList<int> lastRects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModRingOverlay)
};
