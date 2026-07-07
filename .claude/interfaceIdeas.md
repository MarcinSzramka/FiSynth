🎛 Pomysły interfejsowe — co da się wykręcić z tego, co już mamy

Filozofia: silnik ma już unikat (9 trybów stretcha, 4 obwiednie wielopunktowe z playheadami per głos, mod-sloty, sync do tempa). GUI powinno to POKAZYWAĆ — φ jako język wizualny, nie tylko DSP. Poniżej od najtańszych do najambitniejszych.

1. Klawiatura on-screen + podświetlanie głosów — koszt: mały
   juce::MidiKeyboardComponent na dole okna. envPlayheadNote[] już niesie numery grających nut → klawisze podświetlane kolorami playheadColour(slot), spójnie z kropkami na obwiedni. Gra się myszą bez kontrolera MIDI; kompilowany test-mode przestaje być jedyną drogą odsłuchu.

2. Pierścienie modulacji na gałkach ("mod rings", styl Vital/Serum) — koszt: średnio-mały
   Wokół slidera-celu (cutoff/rezonans/stretch) łuk w kolorze obwiedni-źródła pokazujący zakres amount; po łuku biega kropka = aktualna wartość obwiedni na żywo. Cała infrastruktura istnieje: kolory envelopeColour(), czasy envPlayheadTime[env][slot], poziom liczy fiEnvEvalRange() (nagłówek EnvelopeModel.h dostępny w GUI). Modulacja przestaje być "ślepa" — widać CO i JAK MOCNO rusza.

3. Przycisk φ-cascade w pasku obwiedni — koszt: trywialny
   Generator punktów w czasach 1/φᵏ i poziomach 1/φᵏ (pomysł #9 z developIdeas) — jednorazowy setter na EnvelopeModel + commitEnvelope. Bonus: przyciski "×φ" / "÷φ" skalujące czasy całej obwiedni — convertEnvelopeTimes() już istnieje.

4. Spirala φ — żywy wizualizer partiali (centerpiece + branding) — koszt: średni
   Spirala logarytmiczna, pełny obrót = ×φ częstotliwości. Partial = kropka: kąt z log_φ(ratio), wielkość/jasność = amplituda, kolor per oscylator. Punchline: w trybie Golden przy stretch=1 wszystkie partiale ustawiają się na JEDNYM promieniu spirali (φⁿ = równe obroty); suwak stretch animuje wędrówkę kropek z pozycji harmonicznych do celu. Dane w 100% deterministyczne z parametrów: ratioRow() (wynieść z SynthVoice do wspólnego nagłówka) + harmonicAmp — GUI liczy samo, ZERO lock-free plumbingu na start. Cache tła wg wzorca z EnvelopeEditor (bgCache). Później: jasność kropek pod realne obwiednie (playheady już są).

5. Morph-pad XY dla stretcha — koszt: mały UI, wymaga morph DSP
   Oś X = stretch 0..1, oś Y = ciągła pozycja między sąsiednimi trybami (pomysł #10 — "jeden lerp więcej"). W tle ghost-dots pozycji partiali z ratioRow. Jeden komponent, jeden nowy parametr.

6. Spektrum z siatką φ — koszt: średni
   Analizator FFT (juce::dsp::FFT + FIFO w processBlock — wzorzec z tutoriali JUCE), ale pionowe linie siatki co φⁿ od zagranej nuty, przełączane na oktawy. Widać gołym okiem jak partiale "siadają" na złotej siatce przy stretch=1.

7. Pierścień Fibonacciego — sekwencer/gate — koszt: duży (razem z arpem w DSP)
   Okrąg kroków S/L z podstawień S→SL, L→S (10110101101…); playhead wiruje w sync z tempem (currentBpm już jest atomikiem). Klik w krok = flip, suwak generacji = długość patternu. UI dla pomysłu #4 z developIdeas; test-mode jako szkielet arpa.

8. Pole stereo phyllotaxis — koszt: mały UI (sensowny dopiero z golden unison)
   Okrągły widżet: głos k = kropka pod kątem k·137.5°, promień ∝ detune frac(k·φ)·spread. Od razu widać "słonecznik" pola stereo. Para do pomysłu #2 z developIdeas.

Proponowana kolejność: 1 → 2 → 3 (szybkie wygrane na istniejącej infrastrukturze) → 4 (centerpiece) → 5 → 6 → 7/8 (razem z odpowiednim DSP).

STAN: moduły 1–8 wdrożone 2026-07-06.
- 1–4: VoiceKeyboard.h, ModRingOverlay.h/.cpp, przyciski φ w PluginEditor, SpiralVisualizer.h/.cpp + PartialTables.h (wspólne źródło ratio dla DSP i GUI).
- 5: MorphPad.h/.cpp — parametr oscNstretchmode zmieniony z Choice na CIĄGŁY Float 0..8 (część ułamkowa = crossfade sąsiednich celów w updateStretchDeltas); stare presety wczytują się 1:1, ComboBox działa dalej na tym samym parametrze.
- 6: SpectrumAnalyzer.h/.cpp — FFT 2048/Hann na mono sumie wyjścia (AbstractFifo z processBlock), siatka φⁿ od granej nuty przełączana na oktawy.
- 7: FibGateRing.h/.cpp + applyFibGate w procesorze — trance-gate, pattern = słowo Fibonacciego (5/8/13/21/34 kroków), flips w atomiku uint64 (klik w kropkę), sync do ppq hosta / BPM, wygładzanie 1.5 ms. Parametry: gateOn/gateDepth/gateDiv/gateGen. (Wariant „arpeggiator" z developIdeas #4 wciąż otwarty.)
- 8: PhylloField.h/.cpp + pan partiali w SynthVoice — zamiast unisona: partial n → pan frac(n/φ)·spread (constant power, fundament w centrum), parametr spread, filtr przeszedł na 2 kanały. (Pełny golden unison z detune frac(k·φ) — developIdeas #2 — wciąż otwarty.)
- Bonus: AboutPanel.h/.cpp — pełna dokumentacja w pluginie (przycisk About), 10 sekcji: tor sygnału, wzory trybów, architektura wątków, zależności.

REDESIGN 2026-07-07 — układ "hero-spirala" (wariant D z karty propozycji):
- Spirala φ = CENTRUM i KONTROLER: drag po tarczy steruje stretch (promień)
  i stretchmode (kąt, 9 etykiet trybów wokół tarczy, klik w etykietę = skok
  do trybu); przyciski 1/2/3 wybierają oscylator. MorphPad w 100% wchłonięty
  (pliki MorphPad.* zostały na dysku, ale są POZA buildem); gałki stretch
  i combo trybu zniknęły z paneli oscylatorów.
- Ring modulacji Stretch 1–3 rysowany wzdłuż promienia znacznika na spirali
  (ModRingOverlay::Targets.spiral + SpiralVisualizer::stretchPointFor).
- Layout: osc = 3 panele w lewej kolumnie, spirala 500px w centrum,
  obwiednie/filtr/LFO/φ-engine w prawej kolumnie, dolny pas gate|widmo|stereo.
- Paleta: FiLook.h → fiCol (ciepłe brązy, złoto tylko jako akcent φ)
  + FiLookAndFeel (złote gałki/suwaki/comba) ustawiany na content edytora.
  Kolory oscylatorów: złoto / patyna / terakota. Tło edytora obwiedni
  ściemnione do ciepłej czerni (kolor niesie krzywa).
