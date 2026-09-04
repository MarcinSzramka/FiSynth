#pragma once

#include <JuceHeader.h>
#include <array>

// === Wspólne tablice partiali (jedno źródło prawdy dla DSP i GUI) ===
//
// fiRatioRow zwraca tablicę ratio częstotliwości partialek: freq = baseFreq ·
// ratio[wiersz][n]. Wiersz 0 = szereg harmoniczny (n+1), wiersze 1..9 = cele
// stretchMode 0..8. SynthVoice liczy z niej deltaU (brzmienie), a
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

// Czy value występuje w rosnącym ciągu (maski Fibonacci/Lucas Comb).
inline bool fiSeqContains (const int* seq, int value) noexcept
{
    for (int i = 0; i < fiNumPartials && seq[i] <= value; ++i)
        if (seq[i] == value)
            return true;
    return false;
}

// Pierwsze 16 liter nieskończonego słowa Fibonacciego (S→SL, L→S), S = true.
// To samo słowo napędza gate — tu moduluje amplitudy partiali (wave Fib Word).
inline constexpr bool fiWord16[fiNumPartials] =
    { true, false, true, true, false, true, false, true,
      true, false, true, true, false, true, false, true };

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

// Amplituda n-tej partialki danego waveformu. 0..5 = fale klasyczne,
// 6..12 = rodzina φ/masek: struktury, których nie zrobi wykładnik Tiltu
// (dziury, grzebienie, rejestry). Nowy wave = nowy case + pozycja Choice
// NA KOŃCU listy (APVTS trzyma wartości plain — stare presety zostają ważne).
inline float fiHarmonicAmplitude (int waveform, int n) noexcept
{
    const float baseAmp = 1.0f / std::sqrt (1.0f + (float) n);
    const float h = (float) n + 1.0f;   // numer harmonicznej

    switch (waveform)
    {
        case 0:  // Sine
            return (n == 0) ? 1.0f : 0.0f;

        case 1:  // Square — nieparzyste harmoniczne (n+1 = 1,3,5...), n parzyste
            if (n % 2 == 1) return 0.0f;
            return baseAmp / h;

        case 2:  // Triangle — nieparzyste harmoniczne, opadanie 1/n²
            if (n % 2 == 1) return 0.0f;
            return baseAmp / (h * h);

        case 3:  // Sawtooth
            return baseAmp / h;

        case 4:  // Quadratic
            if (n % 2 == 1) return 0.0f;
            return baseAmp / h;

        case 5:  // Noise
            return baseAmp * 0.1f;

        case 6:  // Pulse 25% — sin(hπ/4)/h: nosowy prostokąt z dziurami
                 // co 4. harmoniczną (duty 1/4).
            return baseAmp * std::abs (std::sin (h * juce::MathConstants<float>::pi * 0.25f)) / h;

        case 7:  // Drawbar — organowe rejestry: 1,2,3,4,6,8 z opadającymi
        {        // poziomami, wyżej cisza.
            switch (n + 1)
            {
                case 1: return baseAmp;
                case 2: return baseAmp * 0.8f;
                case 3: return baseAmp * 0.6f;
                case 4: return baseAmp * 0.5f;
                case 6: return baseAmp * 0.4f;
                case 8: return baseAmp * 0.3f;
                default: return 0.0f;
            }
        }

        case 8:  // Even — fundament + parzyste harmoniczne (pustawy, fletowy;
                 // odwrotność square'a).
            if (n == 0) return baseAmp;
            return ((n + 1) % 2 == 0) ? baseAmp / h : 0.0f;

        case 9:  // Golden Pluck — struna szarpnięta w złotym podziale długości
                 // (x₀ = 1/φ): aₕ = |sin(hπ/φ)|/h². 1/φ jest najbardziej
                 // niewymierne, więc żadna harmoniczna nie trafia dokładnie
                 // w węzeł — najrówniejszy możliwy pluck.
            return baseAmp
                 * std::abs (std::sin ((float) (h * juce::MathConstants<double>::pi / fiPhi)))
                 / (h * h);

        case 10: // Fibonacci Comb — grają tylko partiale o numerach
                 // Fibonacciego (1,2,3,5,8,13); rzadkie, harfowe widmo.
            return fiSeqContains (fiFibSeq, n + 1) ? baseAmp / h : 0.0f;

        case 11: // Lucas Comb — maska z ciągu Lucasa (1,3,4,7,11); pustszy dół.
            return fiSeqContains (fiLucasSeq, n + 1) ? baseAmp / h : 0.0f;

        case 12: // Fib Word — saw ważony słowem Fibonacciego: S = pełna,
                 // L = stłumiona; quasi-periodyczny grzebień (to samo słowo
                 // gra w gate).
            return baseAmp * (fiWord16[n] ? 1.0f : 0.45f) / h;

        default:
            return baseAmp;
    }
}
