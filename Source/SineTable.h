#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

// === Tablica sinusa dla gorącej pętli partiali ===
//
// Silnik liczy 3 osc × 16 partiali × 8 głosów (+ bliźniaki unisono) na PRÓBKĘ,
// czyli rzędu 27 mln sinusów na sekundę przy 48 kHz — to był pojedynczy
// największy koszt CPU całego pluginu. Tablica 4096 wpisów z interpolacją
// liniową liczy to ~2.7× taniej od std::sin.
//
// Faza jest bez­znakowym 32-bitowcem: pełny obrót = 2^32, więc zawijanie robi
// samo przepełnienie (zero gałęzi w pętli), a indeks tablicy to zwykły shift.
// Ta reprezentacja ma stałą rozdzielczość na całym zakresie, w przeciwieństwie
// do fazy w radianach, która traciła precyzję przy rosnącej wartości.
//
// Dokładność: błąd interpolacji liniowej to (Δ²/8)·|sin''| = ok. 2.9e-7 przy
// 4096 wpisach, czyli ~130 dB SNR. Zmierzone na całym łańcuchu (bench_voice
// vs implementacja na std::sin): 132 dB, max błąd próbki 5e-8 — wyjście jest
// numerycznie nieodróżnialne od wersji na std::sin.
// Rozmiar 16 KB mieści się w L1D; większa tablica zaczęłaby wypierać cache
// i realnie SPOWALNIAĆ pętlę, nie dokładając słyszalnej dokładności.

inline constexpr int         fiSineBits  = 12;
inline constexpr int         fiSineSize  = 1 << fiSineBits;          // 4096
inline constexpr int         fiSineShift = 32 - fiSineBits;          // 20
inline constexpr juce::uint32 fiSineMask = (1u << fiSineShift) - 1u;
inline constexpr float       fiSineInvFrac = 1.0f / (float) (1u << fiSineShift);

// Ćwierć obrotu w jednostkach fazy — cos(x) = sin(x + π/2).
inline constexpr juce::uint32 fiPhaseQuarter = 0x40000000u;

inline constexpr double fiPhaseFullTurn = 4294967296.0;   // 2^32

// === Akumulator fazy: 64 bity, z czego górne 32 to faza oscylatora ===
//
// Sama faza 32-bitowa nie wystarcza jako AKUMULATOR: przyrost na próbkę jest
// ułamkiem jednostki, więc obcinanie go do liczby całkowitej gubi tę samą
// resztę przy każdej próbce i systematycznie ZANIŻA częstotliwość (zmierzone:
// SNR względem starej implementacji spadał z 99 dB do 76 dB w ciągu 4 s — to
// był dryf fazy, nie szum tablicy). Dolne 32 bity niosą tę resztę, przez co
// błąd strojenia schodzi do ~1e-15 względnie, czyli poniżej precyzji double.
//
// Arytmetyka jest tej samej ceny co przy 32 bitach: jedno mnożenie double,
// jedna konwersja i dodawanie 64-bitowe (na x86-64 tak samo szybkie jak 32-bit).
// Przyrosty trzymamy więc w jednostkach 2^64 = pełny obrót.
inline constexpr double fiDeltaFullTurn    = 18446744073709551616.0;  // 2^64
inline constexpr double fiDeltaHalfTurn    = 9223372036854775808.0;   // 2^63 = π
inline constexpr double fiDeltaQuarterTurn = 4611686018427387904.0;   // 2^62 = π/2

// Górne 32 bity akumulatora = faza do odczytu tablicy.
inline juce::uint32 fiPhaseOf (juce::uint64 acc) noexcept
{
    return (juce::uint32) (acc >> 32);
}

// Wskaźnik do tablicy. Wołać RAZ na blok i trzymać lokalnie: funkcja ma statyk,
// więc każde wywołanie to sprawdzenie strażnika inicjalizacji — w pętli po
// próbkach byłoby to 27 mln zbędnych sprawdzeń na sekundę.
// Konstruktor procesora wymusza pierwszą inicjalizację (jak fiRatioRow), żeby
// nie odpaliła się z locka na wątku audio.
inline const float* fiSineTable() noexcept
{
    // +1 wpis (kopia zerowego) — interpolacja czyta t[idx+1] bez maskowania.
    static const auto table = []
    {
        std::array<float, (size_t) fiSineSize + 1> t {};
        for (size_t i = 0; i <= (size_t) fiSineSize; ++i)
            t[i] = (float) std::sin (juce::MathConstants<double>::twoPi
                                         * (double) i / (double) fiSineSize);
        return t;
    }();

    return table.data();
}

// sin(2π · phase / 2^32) z interpolacją liniową.
inline float fiSin (const float* table, juce::uint32 phase) noexcept
{
    const auto  idx  = (size_t) (phase >> fiSineShift);
    const float frac = (float) (phase & fiSineMask) * fiSineInvFrac;
    const float a    = table[idx];
    return a + (table[idx + 1] - a) * frac;
}
