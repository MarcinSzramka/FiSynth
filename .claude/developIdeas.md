💡 Pomysły rozwojowe (φ / Fibonacci)

Taksonomia "czemu φ tam działa": (a) φ najbardziej niewymierna → brak okresowości
(FM, ring, unison, drift, delay); (b) ciąg Weyla frac(k·φ) → low-discrepancy
"losowość" (fazy startowe, S&H, detune); (c) kąt złoty → równomierne rozkłady
(pan phyllotaxis); (d) samopodobieństwo 1/φᵏ → obwiednie, tilt widma;
(e) 833.09¢ = 1200·log₂φ → melodia/harmonia (goldint, kwantyzacja, arp).

== ZROBIONE (stan 2026-07-07) ==

✔ 9 trybów stretcha + ciągły morph (MorphPad XY, parametr stretchmode 0..8)
✔ Fibonacci word gate (FibGateRing; S→SL, L→S; flipy klikane w pierścień)
✔ Pan phyllotaxis partiali (spread; frac(n/φ) constant-power)
✔ Golden interval per osc (goldint: k·833.09¢)
✔ Wizualizery: spirala φ + widmo z siatką φⁿ
✔ φ-narzędzia obwiedni: φ-casc (kaskada 1/φᵏ), ×φ, ÷φ
✔ Golden Ring — ring-mod osc1×osc2 (param ringMix); sidebandy f1±f2, z goldint=φ
  gęste nieharmoniczne dzwony; najtańsza droga do "FM-owego" brzmienia w silniku
  addytywnym (jedno mnożenie po akumulacji)
✔ Golden FM — sinus f·φ skaluje przyrosty faz wszystkich partiali (param fmAmt,
  destynacja mod "FM"); dewiacja proporcjonalna = stały indeks per partial
✔ Golden Sub — sub-partial f/φ (−833¢; param subLevel), podąża za osc1
✔ Golden Unison — bliźniaki 8 dolnych partiali rozstrojone frac(n·φ)·0.6%,
  pan odbity L↔R (param unison); chór bez okresowego phasingu
✔ Golden Tilt — wykładnik opadania amplitud aₙ·(n+1)^(1−tilt) per osc
  (oscNtilt, 0.25..3, neutral=1, detent-warto-φ); destynacje mod "Tilt 1/2/3"
✔ Golden Drift — drugi LFO o rate·φ miksowany parametrem lfoDrift; suma nigdy
  się nie powtarza (quasi-periodyczny drift analogowy)
✔ Golden S&H — 4. kształt LFO: schodki frac(k/φ) (Weyl; nieokresowy "random")
✔ Kwantyzacja pitch-modu do 833.09¢ (pitchQuant; toggle przy obwiedni)
✔ Fibonacci arpeggiator — od najniższego klawisza interwały 0,1,2,3,5,8,13,21
  półtonów (dalej mod 24), cykl 5/8/13 kroków, zegar jak gate (arpOn/arpDiv/arpLen)
✔ Golden Delay — multi-tap: 4 tapy w czasach t·φ⁻³..t, wzmocnienia po 1/φ,
  cross-feedback (ping-pong), sync do tempa lub ms (dlyOn/dlySync/dlyDiv/dlyTime/
  dlyFeedback/dlyMix); złote odstępy = zero okresowego fluteru grzebieniowego
  [zweryfikowane cepstrum: piki 33/47/80/130 ms przy t=130 ms]

== DO ZROBIENIA (wg opłacalności) ==

1. Golden Scale (skala 833¢ Bohlena) — kwantyzacja klawiatury/wysokości do
   skali zbudowanej na tonach kombinacyjnych φ; rozszerza goldint z interwału
   do systemu stroju. Mały/średni koszt.
2. Golden FDN reverb — sieć feedback-delay o długościach linii w proporcjach φ
   (wzajemnie niewspółmierne → brak dzwoniących modów); naturalne rozwinięcie
   Golden Delay, ten sam argument matematyczny. Duży koszt (infrastruktura FX
   już jest — delay przetarł szlak).
3. Rytm arpa ze słowa Fibonacciego — kroki S/L arpeggiatora z tego samego
   patternu co gate (teraz rytm równy; gate nałożony daje podobny efekt tanio).
4. Pełne unisono głosów — K kopii całego stosu partiali z detune frac(k·φ)·spread;
   drogie w silniku addytywnym (K× koszt), bliźniaki partiali już dają 80% efektu.
5. Golden wavefolder — progi zagięcia w 1/φᵏ; nieharmoniczny "fold" jako drive.
   Średni koszt, efekt niepewny (może gimmick).
6. Phyllotaxis granular — ziarna rozmieszczane kątem złotym w czasie i panoramie;
   osobny silnik, największy projekt. Wizja na później.

UWAGA — zmiana brzmienia (2026-07-07): fiHarmonicAmplitude poprawia parzystość
harmonicznych Square/Triangle. Stary kod (sprzed PartialTables.h) grał
harmoniczne 2,4,6… BEZ fundamentu — błąd; teraz gra poprawnie 1,3,5….
Sesje/presety z Square/Triangle zapisane na starym kodzie brzmią po
aktualizacji o oktawę niżej i pełniej. Kod zostaje — wpis do release notes.

Notatki: silnik jest addytywny (16 partiali × 3 osc, fazy per partial) — klasyczne
PM per partial jest drogie; Golden FM zrobione przez wspólny mnożnik przyrostów
faz (1 mul/sample, wchodzi w istniejący pitchFactor). Nowe parametry APVTS
dopisywane ZAWSZE na końcu list Choice (stan trzyma wartości plain — stare
presety zostają ważne). Stare presety brzmią identycznie: wszystkie nowe moduły
domyślnie wyłączone/neutralne.
