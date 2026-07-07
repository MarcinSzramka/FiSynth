#include "AboutPanel.h"
#include "FiLook.h"

// Treść jako surowe literały UTF-8 przez String::fromUTF8 — plik jest UTF-8,
// a GCC/Clang trzymają exec-charset w UTF-8, więc bajty docierają nietknięte.
// (Goły String(const char*) z nie-ASCII wpada w asercję — patrz juce_String.cpp:327.)
static juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

std::vector<AboutPanel::Section> AboutPanel::makeSections (bool english)
{
    std::vector<Section> s;

    if (english)
    {
        s.push_back ({ utf8 ("Overview"), utf8 (
            "FiSynth — a subtractive/additive synthesizer built around the golden ratio φ = 1.618…\n"
            "\n"
            "SIGNAL PATH\n"
            "MIDI (on-screen keyboard / host) → Fibonacci arpeggiator (optional) → 8 voices.\n"
            "Each voice:\n"
            "  1. 3 additive oscillators — 16 sine partials each, with the frequency layout\n"
            "     driven by Stretch (see the Oscillators section) and spectral slope by Tilt,\n"
            "  2. Golden Unison — the lower partials get a twin detuned by the golden angle,\n"
            "  3. Golden Ring (osc1×osc2), Golden FM (modulator at f·φ) and a Sub at f/φ,\n"
            "  4. phyllotaxis panning — every partial has a fixed stereo spot from the golden angle,\n"
            "  5. state-variable filter SVF (LP/BP/HP) — shared coefficients for L/R,\n"
            "  6. VCA — the Amp envelope scales the signal.\n"
            "Voice sum → Fibonacci gate → Golden Delay → master gain (ramped) → output.\n"
            "\n"
            "MODULATION\n"
            "3 modulation envelopes (Env 1–3) routed via slots to: cutoff, pitch, osc mix,\n"
            "resonance, stretch 1–3, FM index and tilt 1–3. Plus an LFO on cutoff with four\n"
            "shapes (sine/square/triangle/Golden S&H) and a Golden Drift control.\n"
            "\n"
            "Everything you see in the GUI is computed from the same tables and formulas as the\n"
            "sound — the spiral (which is also the stretch controller) and the spectrum are not\n"
            "illustrations but a live view of the actual engine state.\n"
            "\n"
            "Created and developed by Szramcode — Marcin Szramka.") });

        s.push_back ({ utf8 ("Oscillators & φ-stretch"), utf8 (
            "Each oscillator is a bank of 16 partials: freq(n) = f0 · ratio(n); amplitudes come\n"
            "from the waveform. Two families: CLASSIC (Sine/Square/Triangle/Saw/Quadratic/Noise)\n"
            "and φ/FIBONACCI — spectral structures a tilt exponent cannot make: Pulse 25%\n"
            "(nulls every 4th harmonic), Drawbar (organ registers 1,2,3,4,6,8), Even (fundamental\n"
            "+ even harmonics), Golden Pluck (string plucked at the golden section x₀=1/φ —\n"
            "|sin(hπ/φ)|/h², no harmonic ever hits a node exactly), Fib Comb & Lucas Comb (only\n"
            "partials numbered 1,2,3,5,8,13 / 1,3,4,7,11 play) and Fib Word (amplitudes weighted\n"
            "by the Fibonacci word — the same one that drives the gate).\n"
            "Partials above Nyquist are skipped (zero aliasing).\n"
            "\n"
            "STRETCH — the heart of FiSynth. The knob (0..1) drags the partial ratios from the\n"
            "harmonic series (n+1) towards the target of the selected mode:\n"
            "  • Golden         f·φⁿ — fully inharmonic, bells/metal\n"
            "  • Fibonacci      f·{1,2,3,5,8,13…} — tonal bottom, inharmonic top\n"
            "  • Golden Octave  f·φ^log2(n+1) — the octave becomes a φ step; soft, compressed\n"
            "  • Golden Detune  harmonics micro-detuned by frac(n/φ)·1.2% — chorus/width\n"
            "  • Silver         f·(1+√2)ⁿ  |  • Bronze  f·((3+√13)/2)ⁿ — harsher metals\n"
            "  • Golden Stiff   stiff-string formula with exponent φ — piano-like stretch\n"
            "  • Lucas          f·{1,3,4,7,11…} — Fibonacci's sister, hollower bottom\n"
            "  • Golden Shift   f·(n+1) + f/φ — equal spacing, non-integer ratios, clang\n"
            "\n"
            "THE MODE IS CONTINUOUS (0..8): the fractional part crossfades between neighbouring\n"
            "targets. Both are controlled ON THE SPIRAL in the centre: drag the dial — radius =\n"
            "stretch, angle = mode (9 labels around the rim; clicking a label snaps to that\n"
            "mode). Buttons 1/2/3 pick the oscillator, ghost markers show the others.\n"
            "\n"
            "GOLDEN TILT: spectral slope per oscillator — amplitudes are multiplied by\n"
            "(n+1)^(1−tilt). 1.0 = the waveform's natural spectrum, above 1 darkens (φ ≈ 1.618\n"
            "being the on-theme sweet spot), below 1 brightens. A modulation target (Tilt 1–3),\n"
            "so an envelope can open the spectrum like a filter — but by reshaping the partials\n"
            "themselves.\n"
            "\n"
            "GOLDEN INT (φ): a coarse interval of ±k·833.09 cents (1200·log2 φ) per oscillator —\n"
            "lets you build golden chords across the 3 oscillators. Detune: fine ±48 cents.\n"
            "\n"
            "Partial start phases are spread by the golden angle frac(n/φ)·2π — click-free attack\n"
            "(low crest factor), deterministic: the same note always sounds the same.") });

        s.push_back ({ utf8 ("Golden engine"), utf8 (
            "One violet section, five ways φ shapes the sound. The common thread: φ is the\n"
            "\"most irrational\" number, so nothing built on it ever repeats periodically.\n"
            "\n"
            "RING (osc1×osc2) — ring modulation of the two oscillators' full partial stacks.\n"
            "The product creates sidebands f1±f2 for every pair of partials; with Golden Int\n"
            "set to φ they are maximally dense and non-repeating — instant bells and metal.\n"
            "The knob is dry/wet; oscillators are ring sources even with their Mix at 0\n"
            "(classic: a modulator you only hear through the product).\n"
            "\n"
            "FM (f·φ) — one sine per voice at φ times the note frequency scales the phase\n"
            "increments of ALL partials. The deviation is proportional to each partial's\n"
            "frequency (a constant modulation index), so the whole spectrum shimmers\n"
            "coherently. Index 0..1, also a modulation target (\"FM\") — put an envelope on it\n"
            "for evolving metallic attacks.\n"
            "\n"
            "SUB (f/φ) — a sub-partial 833.09 cents below the fundamental: the \"golden\n"
            "sub-octave\". Follows oscillator 1's tuning; sits dead centre in the stereo field.\n"
            "\n"
            "UNISON (φ) — the 8 lowest partials get a twin detuned by frac(n·φ)·0.6% max\n"
            "(a low-discrepancy sequence — the same trick as the start phases). The twins are\n"
            "panned mirror-image (L↔R swapped), so the beating spreads across the field:\n"
            "a chorus with no periodic phasing, because the beat rates never line up.\n"
            "\n"
            "GOLDEN DELAY — a multi-tap delay with 4 taps at t/φ³, t/φ², t/φ and t, levels\n"
            "falling by 1/φ towards the earlier taps. Golden spacing means the reflections are\n"
            "mutually incommensurate — no periodic comb flutter, just a dense, smooth tail.\n"
            "Feedback is cross-coupled (ping-pong). Time: synced note divisions or free ms\n"
            "(30–1500); enabling/disabling clears the line, so it always starts clean.") });

        s.push_back ({ utf8 ("Multi-point envelopes"), utf8 (
            "4 envelopes (Amp + Env 1–3), each an arbitrary list of points (time, level,\n"
            "curvature) — Vital/Serum style, not a fixed ADSR.\n"
            "\n"
            "EDITING\n"
            "  • drag a point — change its time/level\n"
            "  • double-click empty space — new point; double-click a point — delete it\n"
            "  • Shift+click — set the SUSTAIN point (the envelope waits there while the key\n"
            "    is held; points after it play as the release)\n"
            "  • drag the handle in the middle of a segment — curvature (exp/log)\n"
            "  • double-click the handle — reset to linear\n"
            "\n"
            "φ BUTTONS\n"
            "  • φ-casc — generates a cascade: 50 ms attack, then levels 1/φᵏ in segments,\n"
            "    each shorter than the previous by φ. Self-similar decay — sounds like a\n"
            "    plucked string; sustain sits at the end (level 0), so the note rings out.\n"
            "  • ×φ / ÷φ — scale all times of the active envelope by the golden ratio.\n"
            "\n"
            "833¢ QUANT (next to the φ tools): quantizes the Pitch modulation to multiples of\n"
            "the golden interval 833.09¢ — an envelope sweeping the pitch then steps through\n"
            "golden intervals instead of gliding (consistent with Golden Int).\n"
            "\n"
            "SYNC: with Sync on, point times become BEATS and follow the tempo (DAW or manual\n"
            "BPM); grid 1/4…1/16T with Snap. Playheads: every sounding note draws its own line\n"
            "in its own colour — the same colours as the keyboard highlights.") });

        s.push_back ({ utf8 ("Modulation & mod rings"), utf8 (
            "3 routing slots: Env N → target with depth (Amt, −1..+1). Targets and formulas\n"
            "(identical in the DSP and in the ring drawing):\n"
            "  • Filter Cutoff   f · 2^(4·a)  — up to ±4 octaves\n"
            "  • Pitch           ±24 semitones (a·24); optionally quantized to 833.09¢\n"
            "  • Osc Mix         multiplier max(0, 1+a) on the oscillator sum\n"
            "  • Resonance       multiplier max(0, 1+a)\n"
            "  • Stretch 1/2/3   added directly to the stretch knob (0..1)\n"
            "  • FM              added to the Golden FM index (0..1)\n"
            "  • Tilt 1/2/3      added to the tilt exponent (±1.5 at full depth)\n"
            "where a = Amt · envelope value (0..1).\n"
            "\n"
            "MOD RINGS: next to the target control an arc/line is drawn in the COLOUR of the\n"
            "source envelope (Env1 blue, Env2 yellow, Env3 violet) showing the parameter's\n"
            "full range of motion, with dots running along it — one per sounding voice, at the\n"
            "envelope's value at that voice's playhead. Modulation stops being blind: you see\n"
            "what moves and how far before you hear it.\n"
            "\n"
            "Mods affecting the oscillators (pitch, mix, FM, stretch) are computed per sample;\n"
            "cutoff/resonance/tilt at control rate (every 32 samples) — sufficient there, and\n"
            "the filter's coefficient update is expensive.") });

        s.push_back ({ utf8 ("Filter & LFO"), utf8 (
            "FILTER: StateVariableTPT (topology-preserving transform) — stable under fast\n"
            "modulation. Types: low-pass / band-pass / high-pass. Cutoff 20 Hz–20 kHz\n"
            "(logarithmic), resonance 0.1–10. The filter runs 2 channels (same coefficients,\n"
            "separate L/R state). State resets on every note — a high-resonance ring never\n"
            "bleeds into the next attack.\n"
            "\n"
            "LFO: four shapes — sine, square, triangle and GOLDEN S&H: stepped values from the\n"
            "Weyl sequence frac(k/φ), a \"random\" that never repeats and covers the range\n"
            "evenly (low-discrepancy). Rate 0.1–20 Hz or tempo-synced (1/1…1/16T from the\n"
            "current BPM). Target: cutoff, depth ±50%.\n"
            "\n"
            "GOLDEN DRIFT (φ): blends in a second LFO running at rate·φ. Because the ratio is\n"
            "irrational, the sum never returns to the same phase — quasi-periodic motion like\n"
            "analog drift. At full drift the two are mixed equally; with Golden S&H you get\n"
            "two golden step sequences at φ-related rates layered on top of each other.") });

        s.push_back ({ utf8 ("Gate & arpeggiator"), utf8 (
            "GATE — a trance-gate on the synth output whose pattern is the FIBONACCI WORD:\n"
            "start from S and substitute S→SL, L→S:\n"
            "  S → SL → SLS → SLSSL → SLSSLSLS → …\n"
            "Successive word lengths are Fibonacci numbers — hence the 5/8/13/21/34 step\n"
            "choices. The word never loops internally (it is aperiodic), so the gate grooves\n"
            "without loop monotony yet still sounds coherent.\n"
            "\n"
            "  • S (full dot) = open step, L (hollow) = attenuated by Depth\n"
            "  • click a dot on the ring = flip that step (saved in the preset)\n"
            "  • Step: 1/4…1/16T — step length as a note division\n"
            "  • sync: while the host plays, the gate locks to its bar position (ppq);\n"
            "    without transport it freewheels from the current BPM\n"
            "  • step edges smoothed ~1.5 ms — zero clicks\n"
            "The hand on the ring shows the current step in real time.\n"
            "\n"
            "ARPEGGIATOR (Arp φ, top bar) — while enabled, held keys only choose the root\n"
            "(the lowest one); the notes are generated by a step clock (same kind as the\n"
            "gate's). The melody walks FIBONACCI INTERVALS from the root: 0,1,2,3,5,8,13,21\n"
            "semitones (later terms folded mod 24 to stay in register), cycling over 1/2/3/5/8/13\n"
            "steps. MODES: Up, Down, Up-Down In/Ex (In repeats the turnaround steps),\n"
            "Random φ (Weyl steps frac(k/φ) — non-repeating and locked to the bar\n"
            "position, so DAW loops play identically), true Random and Fib Walk (the\n"
            "position hops through the cycle by successive Fibonacci numbers — a\n"
            "self-similar wander, computed via the Pisano period so it stays\n"
            "transport-locked).\n"
            "WORD toggle: step durations follow the Fibonacci word (S = 1 division,\n"
            "L = 2 — 13 notes over 18 divisions, an aperiodic groove). VEL φ: step\n"
            "velocities walk the Weyl sequence frac(k·φ) — deterministic humanize\n"
            "that never loops. Combine with the gate for golden rhythm on top of\n"
            "golden melody — the patterns interlock without ever repeating in sync.") });

        s.push_back ({ utf8 ("Stereo phyllotaxis"), utf8 (
            "Instead of classic unison — PARTIAL panning. Every partial n has a fixed spot in\n"
            "the stereo field: frac(n/φ) mapped to L..R, the fundamental (n=0) always centred.\n"
            "It is the same trick as sunflower seed packing: the golden angle gives a maximally\n"
            "even distribution — no clumps and no periodic patterns.\n"
            "\n"
            "The SPREAD slider (0..1) scales the width: 0 = mono (exactly the old signal path),\n"
            "1 = full field. Panning is constant-power (gL=√(1−p), gR=√(1+p)), so loudness does\n"
            "not depend on spread. The bottom stays solidly centred while the top of the\n"
            "spectrum opens outwards — width without chorus blur.\n"
            "\n"
            "Golden Unison twins are panned mirror-image to their main partials (L↔R swapped),\n"
            "so the unison beating breathes across the stereo field.\n"
            "\n"
            "Practical note: as with any constant-power panning, a mono fold-down slightly\n"
            "attenuates extreme-panned partials (up to −3 dB at full spread). When checking\n"
            "a mix in mono, compare against spread = 0.\n"
            "\n"
            "The STEREO widget shows every partial's position (height = partial number,\n"
            "size = amplitude).") });

        s.push_back ({ utf8 ("Visualizers"), utf8 (
            "φ SPIRAL (centre): a logarithmic spiral where ONE FULL TURN = ×φ in frequency\n"
            "(angle = log_φ(f/f0)·2π). Every partial is a dot: colour = oscillator, size =\n"
            "amplitude; grey ghosts are the harmonic series positions. In Golden mode at\n"
            "stretch=1 all partials sit on ONE ray (φⁿ = whole turns) — you can literally see\n"
            "how the modes differ. Partials above Nyquist vanish, exactly as in the engine.\n"
            "The fundamental f0 = the currently played note (A2 when idle).\n"
            "The spiral is also the CONTROLLER for stretch and mode — see the Oscillators\n"
            "section; stretch modulation rings are drawn along the marker's radius.\n"
            "\n"
            "SPECTRUM: 2048-point FFT with a Hann window on what actually reaches the output\n"
            "(after the gate, delay and gain). Switchable grid: lines every φⁿ from the played\n"
            "note (golden) or every octave. On the golden grid the partials of stretched modes\n"
            "land on the lines — proof and teaching aid in one. Decay ~36 dB/s (peak-decay),\n"
            "so the picture never flickers.\n"
            "\n"
            "KEYBOARD: mouse-played notes reach the engine as MIDI; keys of currently sounding\n"
            "voices light up in their playhead slot colour — the same hue as the envelope line\n"
            "and the mod-ring dots. One voice = one colour across the whole GUI.") });

        s.push_back ({ utf8 ("Presets & state"), utf8 (
            "A preset = the full state: all parameters + the points of all 4 envelopes + the\n"
            "gate flips.\n"
            "\n"
            "  • Save — writes under a name to ~/.config/FiSynth/Presets/*.fsynth (XML)\n"
            "  • < > — previous/next preset from the list (natural sort)\n"
            "  • New — reset to the init patch (sine, no modulation, gate/delay/arp off)\n"
            "\n"
            "The same serializer handles the DAW session state (getStateInformation), so a host\n"
            "project and a preset file are 100% interchangeable. Old presets load correctly:\n"
            "the stretch mode maps 1:1 onto the continuous parameter, and any parameter missing\n"
            "from the file (e.g. presets saved before the golden engine existed) is explicitly\n"
            "reset to its default — an old preset sounds exactly as it did on the day it was\n"
            "saved, no matter what you had cranked before loading it.\n"
            "\n"
            "The standalone build additionally remembers audio/MIDI devices and window state\n"
            "in ~/.config/FiSynth.settings.") });

        s.push_back ({ utf8 ("Architecture & deps"), utf8 (
            "DEPENDENCIES\n"
            "  • JUCE 8.0.12 (submodule External/JUCE) — modules: juce_audio_utils (keyboard,\n"
            "    standalone), juce_dsp (SVF filter, FFT), juce_audio_processors (APVTS, VST3)\n"
            "  • CMake ≥ 3.22 + Ninja, C++20; formats: VST3 + Standalone (Linux)\n"
            "  • zero other libraries — all the φ math is home-grown (PartialTables.h)\n"
            "\n"
            "THREADING\n"
            "  • Parameters: APVTS — audio reads through a cache of raw atomics (ParamPtrs),\n"
            "    no string lookups inside the audio block.\n"
            "  • Envelopes: the editable model lives on the GUI thread; audio gets a flat POD\n"
            "    snapshot (EnvSnapshot) swapped under a try-locked spinlock — audio never blocks.\n"
            "  • Playheads/notes: arrays of atomics written by audio, read by GUI timers.\n"
            "  • Gate: pattern+flips in a single uint64 (fetch_xor from the GUI), step atomic.\n"
            "  • Arpeggiator: consumes note on/off from the MIDI buffer (held-key set) and\n"
            "    injects its own notes sample-accurately; delay runs on the master bus with\n"
            "    a time-smoothed (~80 ms) interpolated read — knob turns glide, never crunch.\n"
            "  • Spectrum: lock-free AbstractFifo (16k samples) audio→GUI.\n"
            "\n"
            "ONE SOURCE OF TRUTH\n"
            "PartialTables.h holds the ratio tables of all modes and the waveform amplitudes.\n"
            "The engine derives phase increments from them, the spiral/pad/spectrum derive\n"
            "pixels. The GUI cannot lie, because it computes from the same formula as the sound.\n"
            "\n"
            "Code: github — Source/ directory. Development ideas: .claude/developIdeas.md\n"
            "and .claude/interfaceIdeas.md in the repo.") });

        return s;
    }

    s.push_back ({ utf8 ("Przegląd"), utf8 (
        "FiSynth — syntezator subtraktywno-addytywny zbudowany wokół złotej liczby φ = 1.618…\n"
        "\n"
        "TOR SYGNAŁU\n"
        "MIDI (klawiatura ekranowa / host) → arpeggiator Fibonacciego (opcjonalny) → 8 głosów.\n"
        "Każdy głos:\n"
        "  1. 3 oscylatory addytywne — po 16 partiali liczonych sinusami, z rozkładem\n"
        "     częstotliwości sterowanym przez Stretch (sekcja Oscylatory) i nachyleniem\n"
        "     widma przez Tilt,\n"
        "  2. Golden Unison — dolne partiale dostają bliźniaka rozstrojonego kątem złotym,\n"
        "  3. Golden Ring (osc1×osc2), Golden FM (modulator f·φ) i Sub na f/φ,\n"
        "  4. pan phyllotaxis — każdy partial ma stałą pozycję stereo z kąta złotego,\n"
        "  5. filtr stanowy SVF (LP/BP/HP) — wspólne współczynniki dla L/R,\n"
        "  6. VCA — obwiednia amplitudy (Amp) mnoży sygnał.\n"
        "Suma głosów → gate Fibonacciego → Golden Delay → master gain (z rampą) → wyjście.\n"
        "\n"
        "MODULACJA\n"
        "3 obwiednie modulacyjne (Env 1–3) routowane slotami na: cutoff, pitch, osc mix,\n"
        "rezonans, stretch 1–3, indeks FM i tilt 1–3. Do tego LFO na cutoff z czterema\n"
        "kształtami (sine/square/triangle/Golden S&H) i suwakiem Golden Drift.\n"
        "\n"
        "Wszystko, co widzisz w GUI, liczone jest z tych samych tablic i wzorów co dźwięk —\n"
        "spirala (będąca zarazem kontrolerem stretcha) i widmo to nie ilustracje, tylko\n"
        "podgląd realnego stanu silnika.\n"
        "\n"
        "Created and developed by Szramcode — Marcin Szramka.") });

    s.push_back ({ utf8 ("Oscylatory i stretch φ"), utf8 (
        "Każdy oscylator to bank 16 partiali: freq(n) = f0 · ratio(n), amplitudy zależą od\n"
        "waveformu. Dwie rodziny: KLASYCZNE (Sine/Square/Triangle/Saw/Quadratic/Noise) oraz\n"
        "φ/FIBONACCI — struktury widmowe, których nie zrobi wykładnik Tiltu: Pulse 25%\n"
        "(dziury co 4. harmoniczną), Drawbar (rejestry organowe 1,2,3,4,6,8), Even (fundament\n"
        "+ parzyste), Golden Pluck (struna szarpnięta w złotym podziale x₀=1/φ — |sin(hπ/φ)|/h²,\n"
        "żadna harmoniczna nie trafia dokładnie w węzeł), Fib Comb i Lucas Comb (grają tylko\n"
        "partiale o numerach 1,2,3,5,8,13 / 1,3,4,7,11) oraz Fib Word (amplitudy ważone słowem\n"
        "Fibonacciego — tym samym, które napędza gate).\n"
        "Partiale ponad Nyquistem są pomijane (zero aliasingu).\n"
        "\n"
        "STRETCH — serce FiSynth. Suwak (0..1) przeciąga ratio partiali od szeregu\n"
        "harmonicznego (n+1) do celu wybranego trybu:\n"
        "  • Golden         f·φⁿ — pełna inharmonia, dzwony/metal\n"
        "  • Fibonacci      f·{1,2,3,5,8,13…} — dół tonalny, góra inharmoniczna\n"
        "  • Golden Octave  f·φ^log2(n+1) — oktawa staje się krokiem φ; miękki, skompresowany\n"
        "  • Golden Detune  harmoniczne mikro-rozstrojone o frac(n/φ)·1.2% — chór/szerokość\n"
        "  • Silver         f·(1+√2)ⁿ  |  • Bronze  f·((3+√13)/2)ⁿ — ostrzejsze metale\n"
        "  • Golden Stiff   struna ze sztywnością, wykładnik φ — fortepianowy stretch\n"
        "  • Lucas          f·{1,3,4,7,11…} — siostra Fibonacciego, pustszy dół\n"
        "  • Golden Shift   f·(n+1) + f/φ — równe odstępy, klangor\n"
        "\n"
        "TRYB JEST CIĄGŁY (0..8): część ułamkowa crossfade'uje między sąsiednimi celami.\n"
        "Oboma steruje się NA SPIRALI w centrum: przeciągnij po tarczy — promień = stretch,\n"
        "kąt = tryb (9 etykiet wokół tarczy; klik w etykietę = skok do tego trybu).\n"
        "Przyciski 1/2/3 wybierają oscylator, duchy-znaczniki pokazują pozostałe.\n"
        "\n"
        "GOLDEN TILT: nachylenie widma per oscylator — amplitudy mnożone przez (n+1)^(1−tilt).\n"
        "1.0 = naturalne widmo waveformu, powyżej 1 przyciemnia (tematyczny punkt to φ ≈ 1.618),\n"
        "poniżej 1 rozjaśnia. Jest celem modulacji (Tilt 1–3), więc obwiednia może otwierać\n"
        "widmo jak filtr — tyle że przekształcając same partiale.\n"
        "\n"
        "GOLDEN INT (φ): gruby interwał ±k·833.09 centów (1200·log2 φ) na oscylator —\n"
        "pozwala budować akordy złote na 3 oscylatorach. Detune: dostrojenie ±48 centów.\n"
        "\n"
        "Fazy startowe partiali są rozłożone kątem złotym frac(n/φ)·2π — atak bez kliku\n"
        "(niski crest factor), deterministycznie: ta sama nuta brzmi tak samo.") });

    s.push_back ({ utf8 ("Złoty silnik"), utf8 (
        "Jedna fioletowa sekcja, pięć zastosowań φ w brzmieniu. Wspólny mianownik: φ jest\n"
        "liczbą \"najbardziej niewymierną\", więc nic, co na niej zbudowane, nie powtarza\n"
        "się okresowo.\n"
        "\n"
        "RING (osc1×osc2) — ring-modulacja pełnych stosów partiali obu oscylatorów.\n"
        "Produkt tworzy sidebandy f1±f2 dla każdej pary partiali; z Golden Int ustawionym\n"
        "na φ są maksymalnie gęste i nierepetujące — natychmiastowe dzwony i metal.\n"
        "Gałka to dry/wet; oscylatory są źródłem ringu nawet przy Mix=0 (klasyka:\n"
        "modulator słyszalny tylko przez produkt).\n"
        "\n"
        "FM (f·φ) — jeden sinus na głos o częstotliwości φ·f nuty skaluje przyrosty faz\n"
        "WSZYSTKICH partiali. Dewiacja jest proporcjonalna do częstotliwości partiala\n"
        "(stały indeks modulacji), więc całe widmo mieni się spójnie. Indeks 0..1, także\n"
        "cel modulacji (\"FM\") — obwiednia na nim daje ewoluujące metaliczne ataki.\n"
        "\n"
        "SUB (f/φ) — sub-partial 833.09 centa pod fundamentem: \"złota sub-oktawa\".\n"
        "Podąża za strojeniem oscylatora 1; siedzi idealnie w centrum panoramy.\n"
        "\n"
        "UNISON (φ) — 8 dolnych partiali dostaje bliźniaka rozstrojonego o frac(n·φ)·0.6%\n"
        "maks. (ciąg low-discrepancy — ta sama sztuczka co fazy startowe). Bliźniaki są\n"
        "panoramowane lustrzanie (L↔R zamienione), więc dudnienie oddycha w polu stereo:\n"
        "chór bez okresowego phasingu, bo tempa dudnień nigdy się nie zgrywają.\n"
        "\n"
        "GOLDEN DELAY — delay multi-tap: 4 tapy w czasach t/φ³, t/φ², t/φ i t, poziomy\n"
        "opadają po 1/φ w stronę wcześniejszych tapów. Złote odstępy są wzajemnie\n"
        "niewspółmierne — brak okresowego fluteru grzebieniowego, tylko gęsty, gładki\n"
        "ogon. Sprzężenie na krzyż (ping-pong). Czas: podział nut w sync albo wolne ms\n"
        "(30–1500); włączenie/wyłączenie czyści linię — zawsze startuje czysto.") });

    s.push_back ({ utf8 ("Obwiednie wielopunktowe"), utf8 (
        "4 obwiednie (Amp + Env 1–3), każda to dowolna lista punktów (czas, poziom,\n"
        "krzywizna) — styl Vital/Serum, nie sztywne ADSR.\n"
        "\n"
        "EDYCJA\n"
        "  • przeciągnij punkt — zmiana czasu/poziomu\n"
        "  • dwuklik na pustym — nowy punkt; dwuklik na punkcie — usunięcie\n"
        "  • Shift+klik — ustawienie punktu SUSTAIN (obwiednia czeka na nim, póki\n"
        "    klawisz wciśnięty; punkty za nim grają jako release)\n"
        "  • przeciągnij uchwyt na środku segmentu — krzywizna (exp/log)\n"
        "  • dwuklik na uchwycie — reset do liniowego\n"
        "\n"
        "PRZYCISKI φ\n"
        "  • φ-casc — generuje kaskadę: atak 50 ms, potem poziomy 1/φᵏ w segmentach,\n"
        "    z których każdy jest krótszy o φ. Samopodobny decay — brzmi jak szarpnięta\n"
        "    struna; sustain na końcu (poziom 0), więc nuta wybrzmiewa sama.\n"
        "  • ×φ / ÷φ — skalują czasy całej aktywnej obwiedni złotym stosunkiem.\n"
        "\n"
        "833¢ QUANT (obok narzędzi φ): kwantyzuje modulację Pitch do krotności złotego\n"
        "interwału 833.09¢ — obwiednia przemiatająca wysokość gra wtedy schodki złotych\n"
        "interwałów zamiast glissanda (spójne z Golden Int).\n"
        "\n"
        "SYNC: przy włączonym Sync czasy punktów stają się BEATAMI i podążają za tempem\n"
        "(DAW albo ręczne BPM); siatka 1/4…1/16T ze Snapem. Playheady: każda grająca nuta\n"
        "rysuje własną kreskę w swoim kolorze — te same kolory co podświetlenie klawiszy.") });

    s.push_back ({ utf8 ("Modulacja i mod-ringi"), utf8 (
        "3 sloty routingu: Env N → cel z głębokością (Amt, −1..+1). Cele i wzory\n"
        "(identyczne w DSP i w rysowaniu pierścieni):\n"
        "  • Filter Cutoff   f · 2^(4·a)  — do ±4 oktaw\n"
        "  • Pitch           ±24 półtony (a·24); opcjonalnie kwantowane do 833.09¢\n"
        "  • Osc Mix         mnożnik max(0, 1+a) na sumie oscylatorów\n"
        "  • Resonance       mnożnik max(0, 1+a)\n"
        "  • Stretch 1/2/3   dodawane wprost do suwaka stretch (0..1)\n"
        "  • FM              dodawane do indeksu Golden FM (0..1)\n"
        "  • Tilt 1/2/3      dodawane do wykładnika tiltu (±1.5 przy pełnej głębokości)\n"
        "gdzie a = Amt · wartość obwiedni (0..1).\n"
        "\n"
        "MOD-RINGI: przy kontrolce-celu rysowany jest łuk/kreska w KOLORZE obwiedni-źródła\n"
        "(Env1 niebieski, Env2 żółty, Env3 fioletowy) pokazujący pełny zakres ruchu\n"
        "parametru, a po nim biegają kropki — po jednej na każdy grający głos, z wartością\n"
        "obwiedni w czasie jego playheada. Modulacja przestaje być ślepa: widać co i jak\n"
        "mocno się rusza, zanim usłyszysz.\n"
        "\n"
        "Mody wpływające na oscylatory (pitch, mix, FM, stretch) liczone są co próbkę;\n"
        "cutoff/rezonans/tilt w tempie kontrolnym (co 32 próbki) — tam wystarcza, a filtr\n"
        "ma drogie przeliczanie współczynników.") });

    s.push_back ({ utf8 ("Filtr i LFO"), utf8 (
        "FILTR: StateVariableTPT (topology-preserving transform) — stabilny przy szybkiej\n"
        "modulacji. Typy: low-pass / band-pass / high-pass. Cutoff 20 Hz–20 kHz (skala\n"
        "logarytmiczna), rezonans 0.1–10. Filtr ma 2 kanały (te same współczynniki,\n"
        "osobny stan L/R). Reset stanu przy każdej nucie — ring wysokiego rezonansu\n"
        "nie wjeżdża w atak następnej.\n"
        "\n"
        "LFO: cztery kształty — sine, square, triangle oraz GOLDEN S&H: schodki o\n"
        "wartościach z ciągu Weyla frac(k/φ), czyli \"random\", który nigdy się nie\n"
        "powtarza i równomiernie pokrywa zakres (low-discrepancy). Rate 0.1–20 Hz albo\n"
        "Sync do tempa (1/1…1/16T z bieżącego BPM). Cel: cutoff, głębokość ±50%.\n"
        "\n"
        "GOLDEN DRIFT (φ): domieszkowuje drugi LFO biegnący z rate·φ. Stosunek jest\n"
        "niewymierny, więc suma nigdy nie wraca do tej samej fazy — quasi-periodyczny\n"
        "ruch jak drift analogu. Przy pełnym drifcie oba LFO mieszają się po równo;\n"
        "z Golden S&H dostajesz dwie złote sekwencje schodków w tempach związanych przez φ.") });

    s.push_back ({ utf8 ("Gate i arpeggiator"), utf8 (
        "GATE — trance-gate na wyjściu syntezatora, którego pattern to SŁOWO FIBONACCIEGO:\n"
        "startujemy od S i podstawiamy S→SL, L→S:\n"
        "  S → SL → SLS → SLSSL → SLSSLSLS → …\n"
        "Długości kolejnych słów to liczby Fibonacciego — stąd wybór 5/8/13/21/34 kroków.\n"
        "Słowo nigdy się nie zapętla wewnętrznie (jest aperiodyczne), więc gate groove'uje\n"
        "bez monotonii pętli, a mimo to brzmi spójnie.\n"
        "\n"
        "  • S (pełna kropka) = krok otwarty, L (pusta) = stłumiony o Depth\n"
        "  • klik w kropkę na pierścieniu = flip kroku (zapisywany w presecie)\n"
        "  • Step: 1/4…1/16T — długość kroku w podziale nut\n"
        "  • sync: gdy host gra, gate siedzi na jego pozycji taktu (ppq); bez transportu\n"
        "    leci wolnobieżnie z bieżącego BPM\n"
        "  • krawędzie kroków wygładzone ~1.5 ms — zero trzasków\n"
        "Wskazówka na pierścieniu pokazuje bieżący krok w czasie rzeczywistym.\n"
        "\n"
        "ARPEGGIATOR (Arp φ, górny pasek) — przy włączonym arp trzymane klawisze tylko\n"
        "wybierają root (najniższy); nuty generuje zegar krokowy (taki sam jak w gate).\n"
        "Melodia idzie INTERWAŁAMI FIBONACCIEGO od roota: 0,1,2,3,5,8,13,21 półtonów\n"
        "(dalsze wyrazy składane mod 24, żeby nie uciec z rejestru), w cyklu 1/2/3/5/8/13\n"
        "kroków. TRYBY: Up, Down, Up-Down In/Ex (In powtarza skrajne kroki na\n"
        "nawrocie), Random φ (schodki Weyla frac(k/φ) — nieokresowe i zaczepione\n"
        "o pozycję taktu, więc loop w DAW gra identycznie), czysty Random oraz\n"
        "Fib Walk (pozycja skacze po cyklu o kolejne liczby Fibonacciego —\n"
        "samopodobne błądzenie, liczone przez okres Pisano, stabilne względem\n"
        "transportu).\n"
        "WORD: czasy kroków ze słowa Fibonacciego (S = 1 podział, L = 2 —\n"
        "13 nut na 18 podziałów, aperiodyczny groove). VEL φ: dynamika kroków\n"
        "ze schodków Weyla frac(k·φ) — deterministyczny humanize bez pętli.\n"
        "W parze z gate'em: złoty rytm na złotej melodii — oba patterny\n"
        "zazębiają się, nigdy nie powtarzając się synchronicznie.") });

    s.push_back ({ utf8 ("Stereo phyllotaxis"), utf8 (
        "Zamiast klasycznego unisona — pan PARTIALI. Każdy partial n ma stałą pozycję\n"
        "w panoramie: frac(n/φ) zmapowane na L..R, fundament (n=0) zawsze w centrum.\n"
        "To ten sam trik co ułożenie nasion słonecznika: kąt złoty daje rozkład\n"
        "maksymalnie równomierny — bez zbitek i bez okresowych wzorów.\n"
        "\n"
        "Suwak SPREAD (0..1) skaluje szerokość: 0 = mono (dokładnie dawny tor),\n"
        "1 = pełne pole. Panning jest constant-power (gL=√(1−p), gR=√(1+p)), więc\n"
        "głośność nie zależy od spread. Dół zostaje stabilnie w środku, góra widma\n"
        "otwiera się na boki — szerokość bez chorusowego rozmycia.\n"
        "\n"
        "Bliźniaki Golden Unison są panoramowane lustrzanie względem swoich partiali\n"
        "głównych (L↔R zamienione), więc dudnienie unisona oddycha w polu stereo.\n"
        "\n"
        "Uwaga praktyczna: jak w każdym panoramowaniu constant-power, fold-down do\n"
        "mono lekko tłumi skrajnie panoramowane partiale (do −3 dB przy pełnym\n"
        "spread). Sprawdzając miks w mono, porównuj brzmienie przy spread = 0.\n"
        "\n"
        "Widżet STEREO pokazuje pozycję każdego partiala (wysokość = numer partiala,\n"
        "wielkość = amplituda).") });

    s.push_back ({ utf8 ("Wizualizery"), utf8 (
        "SPIRALA φ (centrum): spirala logarytmiczna, na której PEŁNY OBRÓT = ×φ częstotliwości\n"
        "(kąt = log_φ(f/f0)·2π). Każdy partial to kropka: kolor = oscylator, wielkość =\n"
        "amplituda; szare duchy to pozycje szeregu harmonicznego. W trybie Golden przy\n"
        "stretch=1 wszystkie partiale leżą na JEDNYM promieniu (φⁿ = całkowite obroty) —\n"
        "widać gołym okiem, czym różnią się tryby. Partiale ponad Nyquistem znikają,\n"
        "dokładnie jak w silniku. Fundament f0 = aktualnie grana nuta (bez grania: A2).\n"
        "Spirala jest też KONTROLEREM stretcha i trybu — patrz sekcja Oscylatory;\n"
        "pierścienie modulacji stretcha rysują się wzdłuż promienia znacznika.\n"
        "\n"
        "WIDMO: FFT 2048 z oknem Hanna na tym, co realnie leci na wyjście (po gate'cie,\n"
        "delayu i gainie). Siatka przełączana: linie co φⁿ od granej nuty (złote) albo co\n"
        "oktawę. Na siatce złotej partiale stretchowanych trybów siadają na liniach —\n"
        "dowód i dydaktyka w jednym. Opadanie ~36 dB/s (peak-decay), więc obraz nie migocze.\n"
        "\n"
        "KLAWIATURA: nuty grane myszą trafiają do silnika jak MIDI; klawisze aktualnie\n"
        "grających głosów świecą kolorem slotu playheada — ta sama barwa co kreska na\n"
        "obwiedni i kropki na mod-ringach. Jeden głos = jeden kolor w całym GUI.") });

    s.push_back ({ utf8 ("Presety i stan"), utf8 (
        "Preset = pełny stan: wszystkie parametry + punkty 4 obwiedni + flipy gate'a.\n"
        "\n"
        "  • Save — zapis pod nazwą do ~/.config/FiSynth/Presets/*.fsynth (XML)\n"
        "  • < > — poprzedni/następny preset z listy (sortowanie naturalne)\n"
        "  • New — reset do patcha init (sinus, bez modulacji, gate/delay/arp off)\n"
        "\n"
        "Ten sam serializer obsługuje stan sesji DAW (getStateInformation), więc projekt\n"
        "hosta i plik presetu są w 100% wymienne. Stare presety wczytują się poprawnie:\n"
        "tryb stretcha przechodzi 1:1 na parametr ciągły, a każdy parametr, którego\n"
        "w pliku nie ma (np. presety sprzed złotego silnika), jest jawnie resetowany do\n"
        "wartości domyślnej — stary preset brzmi dokładnie jak w dniu zapisu, niezależnie\n"
        "od tego, co było podkręcone przed wczytaniem.\n"
        "\n"
        "Wersja standalone dodatkowo pamięta urządzenia audio/MIDI i stan okna\n"
        "w ~/.config/FiSynth.settings.") });

    s.push_back ({ utf8 ("Architektura i zależności"), utf8 (
        "ZALEŻNOŚCI\n"
        "  • JUCE 8.0.12 (submoduł External/JUCE) — moduły: juce_audio_utils (klawiatura,\n"
        "    standalone), juce_dsp (filtr SVF, FFT), juce_audio_processors (APVTS, VST3)\n"
        "  • CMake ≥ 3.22 + Ninja, C++20; formaty: VST3 + Standalone (Linux)\n"
        "  • zero innych bibliotek — cała matematyka φ jest własna (PartialTables.h)\n"
        "\n"
        "ARCHITEKTURA WĄTKÓW\n"
        "  • Parametry: APVTS — audio czyta przez cache surowych atomików (ParamPtrs),\n"
        "    bez lookupów po stringach w bloku audio.\n"
        "  • Obwiednie: model edytowalny żyje w wątku GUI; audio dostaje płaską migawkę\n"
        "    POD (EnvSnapshot) podmienianą pod spinlockiem z try-lockiem — audio nigdy\n"
        "    nie blokuje.\n"
        "  • Playheady/nuty: tablice atomików pisane przez audio, czytane timerami GUI.\n"
        "  • Gate: pattern+flipy w jednym uint64 (fetch_xor z GUI), krok w atomiku.\n"
        "  • Arpeggiator: zjada note on/off z bufora MIDI (zbiór trzymanych klawiszy)\n"
        "    i wstrzykuje własne nuty z dokładnością do próbki; delay pracuje na sumie,\n"
        "    z wygładzonym (~80 ms) interpolowanym czasem — kręcenie gałką płynie,\n"
        "    nigdy nie zgrzyta.\n"
        "  • Widmo: lock-free AbstractFifo (16k próbek) audio→GUI.\n"
        "\n"
        "JEDNO ŹRÓDŁO PRAWDY\n"
        "PartialTables.h trzyma tablice ratio wszystkich trybów i amplitudy waveformów.\n"
        "Silnik liczy z nich przyrosty faz, a spirala/pad/widmo — piksele. GUI nie może\n"
        "skłamać, bo liczy z tego samego wzoru co dźwięk.\n"
        "\n"
        "Kod: github — katalog Source/. Pomysły rozwojowe: .claude/developIdeas.md\n"
        "i .claude/interfaceIdeas.md w repo.") });

    return s;
}

AboutPanel::AboutPanel()
    : sections (makeSections (false))
{
    setWantsKeyboardFocus (false);

    for (int i = 0; i < (int) sections.size(); ++i)
    {
        auto* b = navButtons.add (new juce::TextButton (sections[(size_t) i].title));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (3001);
        b->setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1b1610));
        b->setColour (juce::TextButton::buttonOnColourId, fiCol::goldDim);
        b->onClick = [this, i] { showSection (i); };
        addAndMakeVisible (b);
    }

    closeButton.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeButton);

    // Przełącznik języka: napis pokazuje język, NA KTÓRY przełączy klik.
    langButton.onClick = [this] { setLanguage (! english); };
    addAndMakeVisible (langButton);

    content.setMultiLine (true);
    content.setReadOnly (true);
    content.setCaretVisible (false);
    content.setScrollbarsShown (true);
    content.setLineSpacing (1.25f);
    content.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff14181f));
    content.setColour (juce::TextEditor::textColourId,       juce::Colours::whitesmoke);
    content.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xff2e2b22));
    content.setFont (juce::Font (juce::FontOptions (14.5f)));
    addAndMakeVisible (content);

    setLanguage (false);
    navButtons[0]->setToggleState (true, juce::dontSendNotification);
    showSection (0);
}

void AboutPanel::setLanguage (bool en)
{
    english  = en;
    sections = makeSections (english);

    // Tytuły nawigacji, przycisk zamknięcia i etykieta przełącznika podążają
    // za językiem; liczba sekcji jest identyczna w obu wersjach.
    jassert ((int) sections.size() == navButtons.size());
    for (int i = 0; i < navButtons.size(); ++i)
        navButtons[i]->setButtonText (sections[(size_t) i].title);

    closeButton.setButtonText (english
        ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95 Close"))
        : juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95 Zamknij")));
    langButton.setButtonText (english ? "PL" : "EN");

    showSection (currentSection);
    repaint();   // tytuł karty rysowany w paint()
}

void AboutPanel::showSection (int idx)
{
    currentSection = juce::jlimit (0, (int) sections.size() - 1, idx);
    content.setText (sections[(size_t) currentSection].body, juce::dontSendNotification);
    content.setCaretPosition (0);
    content.scrollEditorToPositionCaret (0, 0);
}

void AboutPanel::resized()
{
    card = getLocalBounds().reduced (juce::jmax (16, getWidth() / 14),
                                     juce::jmax (16, getHeight() / 16));

    auto inner = card.reduced (14);
    auto header = inner.removeFromTop (30);
    closeButton.setBounds (header.removeFromRight (96).reduced (0, 2));
    header.removeFromRight (6);
    langButton.setBounds (header.removeFromRight (44).reduced (0, 2));

    inner.removeFromTop (8);
    auto nav = inner.removeFromLeft (190);
    for (auto* b : navButtons)
        nav.removeFromTop (2), b->setBounds (nav.removeFromTop (28));

    inner.removeFromLeft (12);
    content.setBounds (inner);
}

void AboutPanel::paint (juce::Graphics& g)
{
    // Przyciemnione tło + karta.
    g.fillAll (juce::Colour (0xd9000000));
    g.setColour (juce::Colour (0xff181510));
    g.fillRoundedRectangle (card.toFloat(), 10.0f);
    g.setColour (fiCol::gold);
    g.drawRoundedRectangle (card.toFloat(), 10.0f, 1.2f);

    g.setColour (juce::Colours::whitesmoke);
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    g.drawText (english
                    ? juce::String (juce::CharPointer_UTF8 ("Fi\xcf\x86Synth \xe2\x80\x94 how it works"))
                    : juce::String (juce::CharPointer_UTF8 ("Fi\xcf\x86Synth \xe2\x80\x94 jak to dzia\xc5\x82""a")),
                card.reduced (14).removeFromTop (30),
                juce::Justification::centredLeft);
}

void AboutPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition()))
        setVisible (false);
}
