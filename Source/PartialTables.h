#pragma once

#include <JuceHeader.h>
#include <array>

// === Wspólne tablice partiali (jedno źródło prawdy dla DSP i GUI) ===
//
// fiRatioRow zwraca tablicę ratio częstotliwości partialek: freq = baseFreq ·
// ratio[wiersz][n]. Wiersz 0 = szereg harmoniczny (n+1), wiersze 1..9 = cele
// stretchMode 0..8. SynthVoice liczy z niej angleDelta (brzmienie), a
// SpiralVisualizer pozycje kropek (obraz) — dzięki temu wizualizacja z definicji
// pokazuje dokładnie to, co gra silnik.

static constexpr int fiNumPartials = 16;

// Złota liczba — jedyna definicja w projekcie (DSP i GUI liczą z tej samej).
inline constexpr double fiPhi = 1.6180339887498949;   // (1+√5)/2

// Liczba trybów stretcha (wiersze 1..fiNumStretchModes tablicy ratio; wiersz 0
// to szereg harmoniczny). Nowy tryb = nowy wiersz tabeli + nazwa poniżej —
// wszystkie zakresy parametrów, kąty na spirali i clampy liczą się z tej stałej.
inline constexpr int fiNumStretchModes = 9;

inline const char* const fiStretchModeNames[fiNumStretchModes] = {
    "Golden", "Fibonacci", "GoldOct", "GoldDet",
    "Silver", "Bronze", "GoldStiff", "Lucas", "GoldShift"
};

// Złoty interwał w centach: 1200·log2(φ) — jedyna pisownia w projekcie
// (goldint, kwantyzacja pitch-modu i pozycje na spirali liczą z tej samej).
inline constexpr float fiGoldenIntervalCents = 833.090296f;

// Indeks Choice "goldint" (0..4) -> przestrojenie w centach (k = idx-2).
inline float fiGoldIntCents (int choiceIdx) noexcept
{
    return (float) (choiceIdx - 2) * fiGoldenIntervalCents;
}

// Pozycja pan partiala n w [-1,1]: 0 dla fundamentu (stabilny dół), dalej
// frac(n/φ) zmapowane liniowo — kąt złoty daje równomierne pole stereo bez
// zbitek. Używane przez SynthVoice (dźwięk) i PhylloField (obraz).
inline float fiGoldenPan (int n) noexcept
{
    if (n <= 0)
        return 0.0f;

    const double g = n / fiPhi;
    return (float) (2.0 * (g - std::floor (g)) - 1.0);
}

// Ciąg Fibonacciego dla celu stretcha: 1,2,3,5,8,... (pomijamy duplikat 1 z
// klasycznego 1,1,2,...). Partialki 0..2 (1,2,3·f) pokrywają się z szeregiem
// harmonicznym -> niski rejestr zostaje tonalny, dopiero górne partialki
// rozjeżdżają się inharmonicznie.
inline constexpr int fiFibSeq[fiNumPartials] =
    { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597 };

// Ciąg Lucasa (siostrzany do Fibonacciego, też zbiega do φ): 1,3,4,7,11,...
// Przeskok 1->3 (pomija 2) -> niski rejestr brzmi bardziej "pusto/kwintowo".
inline constexpr int fiLucasSeq[fiNumPartials] =
    { 1, 3, 4, 7, 11, 18, 29, 47, 76, 123, 199, 322, 521, 843, 1364, 2207 };

// Maks. głębokość mikro-rozstrojenia w trybie Golden Detune (przy stretch=1).
// ~1.2% ≈ ±20 centów na skrajnych partialkach -> bujny, "chórowy" charakter.
inline constexpr float fiGoldenDetuneDepth = 0.012f;

inline constexpr float fiSilverMean   = 2.41421356f;  // 1+√2
inline constexpr float fiBronzeMean   = 3.30277564f;  // (3+√13)/2
inline constexpr float fiGoldenStiffB = 0.02f;        // współczynnik sztywności (Golden Stiff)

// Tablica ratio częstotliwości partialek. Liczona raz, wspólna dla wszystkich
// głosów i komponentów GUI.
inline const double* fiRatioRow (int row) noexcept
{
    static const auto table = []
    {
        std::array<std::array<double, fiNumPartials>, 10> t {};
        constexpr double phiD = fiPhi;

        for (size_t n = 0; n < (size_t) fiNumPartials; ++n)
        {
            const double h = (double) n + 1.0;                 // numer harmonicznej
            t[0][n] = h;                                       // Harmonic (stretch=0)
            t[1][n] = std::pow (phiD, (double) n);             // Golden        f·φ^n
            t[2][n] = (double) fiFibSeq[n];                    // Fibonacci     f·Fib(n)

            // Golden Octave: oktawa w szeregu harmonicznym -> krok φ.
            // f·φ^log2(n+1): harm 2->φ, 4->φ², 8->φ³ (skompresowany, miękki).
            t[3][n] = std::pow (phiD, std::log2 (h));

            // Golden Detune: harmoniczne mikro-rozstrojone o frac((n+1)/φ)
            // zmapowane na [-1,1]; fundament (n=0) czysty, nie rusza wysokości.
            double det = h;
            if (n > 0)
            {
                const double g    = h / phiD;
                const double frac = g - std::floor (g);
                det = h * (1.0 + (double) fiGoldenDetuneDepth * (2.0 * frac - 1.0));
            }
            t[4][n] = det;

            // Metaliczne średnie (uogólnienie φ): f·σ^n.
            t[5][n] = std::pow ((double) fiSilverMean, (double) n);
            t[6][n] = std::pow ((double) fiBronzeMean, (double) n);

            // Golden Stiff: fizyczny wzór struny ze sztywnością, wykładnik = φ.
            t[7][n] = h * std::sqrt (1.0 + (double) fiGoldenStiffB * std::pow (h, phiD));

            t[8][n] = (double) fiLucasSeq[n];                  // Lucas f·Lucas(n)

            // Golden Shift: harmoniczne przesunięte addytywnie o f/φ (fundament
            // n=0 czysty). Odstępy równe, stosunki niecałkowite -> klangor.
            t[9][n] = (n == 0) ? 1.0 : h + 1.0 / phiD;
        }
        return t;
    }();

    return table[(size_t) juce::jlimit (0, fiNumStretchModes, row)].data();
}

// Ratio wszystkich partiali dla CIĄGŁEGO trybu (0..8; ułamek = crossfade
// sąsiednich celów) i stretcha (0..1; lerp od szeregu harmonicznego do celu).
// Jedyna definicja blendu morpha — silnik liczy z niej przyrosty faz
// (updateStretchDeltas), a spirala pozycje kropek; rozjazd niemożliwy.
inline void fiStretchedRatios (float mode, float stretch, double* out) noexcept
{
    const float  m  = juce::jlimit (0.0f, (float) (fiNumStretchModes - 1), mode);
    const int    mA = (int) m;
    const double mf = (double) (m - (float) mA);
    const double* harm = fiRatioRow (0);
    const double* tA   = fiRatioRow (mA + 1);
    const double* tB   = fiRatioRow (juce::jmin (fiNumStretchModes, mA + 2));

    const double s = juce::jlimit (0.0f, 1.0f, stretch);
    for (int n = 0; n < fiNumPartials; ++n)
    {
        const double target = tA[n] + (tB[n] - tA[n]) * mf;
        out[n] = harm[n] + (target - harm[n]) * s;
    }
}

// Amplituda n-tej partialki danego waveformu (0=Sine..5=Noise).
inline float fiHarmonicAmplitude (int waveform, int n) noexcept
{
    const float baseAmp = 1.0f / std::sqrt (1.0f + (float) n);

    switch (waveform)
    {
        case 0:  // Sine
            return (n == 0) ? 1.0f : 0.0f;

        case 1:  // Square — nieparzyste harmoniczne (n+1 = 1,3,5...), n parzyste
            if (n % 2 == 1) return 0.0f;
            return baseAmp / ((float) n + 1.0f);

        case 2:  // Triangle — nieparzyste harmoniczne, opadanie 1/n²
            if (n % 2 == 1) return 0.0f;
            return baseAmp / (((float) n + 1.0f) * ((float) n + 1.0f));

        case 3:  // Sawtooth
            return baseAmp / ((float) n + 1.0f);

        case 4:  // Quadratic
            if (n % 2 == 1) return 0.0f;
            return baseAmp / ((float) n + 1.0f);

        case 5:  // Noise
            return baseAmp * 0.1f;

        default:
            return baseAmp;
    }
}
