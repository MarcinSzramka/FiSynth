#pragma once

#include <JuceHeader.h>

// === Wspólne wzory modulacji (DSP ↔ mod-ringi) ===
//
// SynthVoice::renderNextBlock liczy nimi dźwięk, a ModRingOverlay piksele —
// jedno źródło prawdy, żeby pierścienie nie kłamały o realnym zakresie ruchu
// parametru (wzorzec jak PartialTables.h dla ratio partiali).
//
// Uwaga: ringi pokazują wkład SLOTU obwiedni; LFO (osobny modulator cutoffa,
// mnożący bazę) celowo nie wchodzi w rysowany zakres.
namespace fiMod
{
    constexpr float pitchRangeSemis = 24.0f;   // ModPitch: ±24 półtony przy a = ±1
    constexpr float tiltModDepth    = 1.5f;    // ModTilt: ±1.5 wykładnika przy a = ±1

    inline float cutoffFactor (float a) noexcept { return std::exp2 (a * 4.0f); }         // ±4 oktawy
    inline float gainFactor   (float a) noexcept { return juce::jmax (0.0f, 1.0f + a); }  // mix i rezonans

    inline float clampCutoff    (float f) noexcept { return juce::jlimit (20.0f, 20000.0f, f); }
    inline float clampResonance (float q) noexcept { return juce::jlimit (0.1f, 10.0f, q); }
    inline float clampTilt      (float t) noexcept { return juce::jlimit (0.25f, 3.0f, t); }

    // Wartość parametru po modulacji a = amount·env — te same wzory liczą
    // dźwięk (SynthVoice) i piksele ringów (ModRingOverlay).
    inline float stretchTarget (float base, float a) noexcept { return juce::jlimit (0.0f, 1.0f, base + a); }
    inline float fmTarget      (float base, float a) noexcept { return juce::jlimit (0.0f, 1.0f, base + a); }
    inline float tiltTarget    (float base, float a) noexcept { return clampTilt (base + a * tiltModDepth); }
}
