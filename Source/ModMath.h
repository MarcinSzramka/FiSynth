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
    constexpr float bendRangeSemis  = 2.0f;    // MIDI pitch bend: ±2 półtony (standard GM)
    constexpr float tiltModDepth    = 1.5f;    // ModTilt: ±1.5 wykładnika przy a = ±1

    inline float cutoffFactor (float a) noexcept { return std::exp2 (a * 4.0f); }         // ±4 oktawy
    inline float gainFactor   (float a) noexcept { return juce::jmax (0.0f, 1.0f + a); }  // mix i rezonans

    // Górna granica cutoffa MUSI zależeć od sample rate. juce::dsp::StateVariableTPTFilter
    // liczy g = tan(pi*fc/sr); powyżej Nyquista argument mija pi/2, g robi się UJEMNE
    // i filtr traci stabilność (przy sr 32 kHz i fc 20 kHz g = -2.41 → stan rośnie
    // ~2.8%/próbkę, czyli +60 dB w ~6 ms, potem Inf/NaN w całym łańcuchu). JUCE ma na
    // to jassert, więc build Debug dodatkowo zatrzymywał hosta. Mnożnik 0.49 zostawia
    // margines przed samą osobliwością tan() w 0.5*sr.
    // sampleRate <= 0 (nieznany, np. przy rysowaniu ringów przed prepareToPlay)
    // zachowuje dawne zachowanie ze stałym sufitem 20 kHz.
    inline float clampCutoff (float f, double sampleRate = 0.0) noexcept
    {
        float upper = 20000.0f;
        if (sampleRate > 0.0)
            upper = juce::jmin (upper, (float) (sampleRate * 0.49));
        upper = juce::jmax (upper, 20.0f);          // jlimit wymaga upper >= lower
        return juce::jlimit (20.0f, upper, f);
    }

    inline float clampResonance (float q) noexcept { return juce::jlimit (0.1f, 10.0f, q); }
    inline float clampTilt      (float t) noexcept { return juce::jlimit (0.25f, 3.0f, t); }

    // Wartość parametru po modulacji a = amount·env — te same wzory liczą
    // dźwięk (SynthVoice) i piksele ringów (ModRingOverlay).
    inline float stretchTarget (float base, float a) noexcept { return juce::jlimit (0.0f, 1.0f, base + a); }
    inline float fmTarget      (float base, float a) noexcept { return juce::jlimit (0.0f, 1.0f, base + a); }
    inline float tiltTarget    (float base, float a) noexcept { return clampTilt (base + a * tiltModDepth); }
}
