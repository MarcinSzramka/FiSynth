1. Instrumenty „prawie akustyczne”

Nie realistyczne emulacje fortepianu czy gitary, tylko instrumenty, które brzmią wiarygodnie fizycznie, choć nie istnieją.

Masz do tego:

Golden Pluck,
Golden Stiff,
kaskady obwiedni φ-casc,
tilt zmieniający widmo,
osobne strojenie partiali,
metaliczny transient przez FM albo Ring.

To jest idealne do robienia instrumentów w stylu:

struna z metalu i drewna,
fortepian z innej planety,
szklana kalimba,
wielki rezonujący idiophone,
futurystyczny cymbał.
2. Metale i dzwony

Tutaj FiSynth powinien wpierdalać większość klasycznych subtraktywnych synthów.

Golden, Silver, Bronze, Lucas, Golden Shift, Ring i FM dają ogromną liczbę nieharmonicznych kombinacji. Co ważne, możesz zostawić początek metaliczny, ale później wrócić obwiednią stretcha do bardziej harmonicznego strojenia. Dzięki temu transient brzmi jak uderzenie, a ogon zachowuje czytelną wysokość.

3. Organiczne stereo

Stereo phyllotaxis nie robi klasycznego „szerzej przez rozstrojenie całych głosów”. Rozsuwa partiale, zostawiając fundament w centrum. Dlatego szczególnie dobrze powinny działać:

pady,
organy,
smyczkowe syntezatorowe,
drony,
szerokie plucki,
jasne klawisze.

Można uzyskać wielką szerokość bez całkowitego rozmycia wysokości dźwięku.

4. Generatywne granie

Arpeggiator, Fibonacci Word, Weyl velocity, Fib Walk, gate i Golden Delay tworzą już prawie osobny instrument kompozycyjny.

To nie powinno być traktowane tylko jako dodatek do presetów. Powinieneś mieć osobną kategorię:

Sequences / Generative

Bo preset może być gotowym muzycznym zachowaniem, a nie tylko barwą.


Proponowana struktura banku

Ja bym zrobił pierwszą fabryczną paczkę około 96 presetów:

Kategoria	Liczba
Bass	12
Lead	10
Pads	12
Plucks	12
Keys & Organs	10
Bells & Mallets	12
Strings & Brass	8
Drones & Atmospheres	8
Sequences & Generative	8
FX	4

Nie próbowałbym od razu robić 300. Lepiej 80–100 presetów, z których każdy pokazuje inną cechę silnika.

BASS
1. Phi Foundation

Najbardziej podstawowy firmowy bas.

Osc 1: Saw, stretch 0–0.05, tilt około 1.5
Osc 2: Square albo Even, oktawa 0, detune −5 centów
Osc 3: Sine, ciszej
Sub f/φ: 35–50%
Golden Unison: 10–15%
Filter: LP, cutoff nisko, mała rezonancja
Amp: natychmiastowy attack, krótki decay, sustain 70–80%
Spread: prawie 0

Charakter: klasyczny analogowy dół, ale sub nie jest oktawą niżej. Daje obcy ciężar, szczególnie gdy nuta główna i f/φ nie tworzą zwyczajnego interwału.

2. Golden Sub Pressure
Osc 1: Sine
Osc 2 i 3: wyłączone lub bardzo cicho
Sub: 70–100%
FM: 2–4%
Filter LP prawie otwarty
Env 1 → FM: krótki pik na początku
Amp z miękkim, około 10 ms atakiem

Czysty sub z ledwo wyczuwalnym złotym transientem. Dobry pod techno, ambient bass i cinematic.

3. Fib Reese
Osc 1: Saw, Golden Detune, stretch 0.25–0.4
Osc 2: Saw, detune +10 centów
Osc 3: Square, detune −10 centów
Golden Unison: 50–70%
Spread: 40–60%
Tilt: 1.4–1.8
Filter LP
LFO triangle → cutoff, wolno
Drift 30–50%

To nie będzie klasyczny reese oparty na dwóch sawach, ale bardziej wielowarstwowy, quasi-aperiodyczny reese.

4. Lucas Hollow Bass
Osc 1: Lucas Comb
Stretch w stronę Lucas, około 0.2–0.35
Osc 2: Sine dla fundamentu
Tilt Osc 1 około 1.7
Filter LP/BP
Env 1 → Stretch 1: krótki ruch dodatni
Ring 5–10%

Pusty dół i dziwna, niepełna góra. Bardzo dobry pod industrial, dark ambient i minimal.

5. Bronze Bite
Osc 1: Square lub Pulse 25%
Stretch: Bronze około 0.15–0.25
Osc 2: Sine, jedna oktawa niżej, jeśli strojenie na to pozwala
FM: 10–20%
Env 1 → FM: szybki decay
Filter LP z rezonansowym kopnięciem
Amp krótki, bez długiego release

Bas z metalicznym początkiem, ale nadal tonalnym korpusem.

6. Talking Partial Bass

Nie masz formantów, ale da się zasymulować ruch mowy zmianą widma.

Osc 1: Saw
Osc 2: Drawbar
Osc 3: Fib Comb
Filter BP
LFO → cutoff, triangle 2–5 Hz
Env 1 → Osc Mix
Env 2 → Tilt 1
Env 3 → Stretch 3
Rezonans średnio wysoki

Nie będzie „wobble dubstep jak Serum”, tylko gadający, spektralny bas.

7. Golden Acid
Osc 1: Saw
Tilt 1.2–1.4
LP, wysoka rezonancja
Env 1 → cutoff, duża dodatnia wartość
Amp krótki decay, niski sustain
Gate 13 lub 21 kroków
Delay krótki i cichy

Bez saturacji nie będzie TB-303, ale dostaniesz czysty, ostry, nieregularny acid. Zewnętrzny distortion zrobi z niego potwora.

8. Ring Engine Bass
Osc 1: Square
Osc 2: Sine lub Triangle
Golden Int Osc 2: +1
Miksy oscylatorów nisko
Ring: 30–50%
Osc 3: Sine jako stabilny fundament
Sub: 20%
Filter LP/BP
Env 1 → Ring się nie da, więc Env → FM dla ruchu ataku
Krótki Amp

Ring pozostaje głównym źródłem charakteru, a Osc 3 pilnuje wysokości.

9. Stiff Wire Bass
Osc 1: Triangle
Golden Stiff, stretch 0.2–0.4
Tilt około φ
Golden Pluck jako Osc 2, bardzo cicho
Env 1 → Stretch 1 z szybkim spadkiem
Amp φ-casc
Delay wyłączony

Brzmi jak gruba napięta stalowa struna.

10. Dark Phyllo Bass
Osc 1: Even
Osc 2: Drawbar
Spread 30–40%
Fundament pozostaje centralny, góra idzie na boki
LP cutoff 200–800 Hz zależnie od nuty
Unison 20%
Sub 25%

Bas szeroki, ale nie rozlazły na samym dole.

LEAD
1. Golden Mono Lead
Saw + Triangle + Sine
Golden Detune 10–20%
Unison 30%
Spread 25%
LP z umiarkowaną rezonancją
LFO sine → cutoff
Delay 15–25%

Klasyczny lead będący punktem wejścia dla ludzi, którzy nie znają jeszcze FiSynth.

2. 833 Hero
Osc 1: Saw
Osc 2: Square, Golden Int +1
Osc 3: Triangle, Golden Int −1
Osc 2 i 3 ciszej niż fundament
Filter LP
Env 1 → Pitch z włączonym 833¢ Quant
Obwiednia schodkowa, kilka poziomów
Delay ping-pong

Lead grający złote ozdobniki i przeskoki zamiast zwykłego pitch envelope.

3. Spiral Screamer
Osc 1: Saw, jasny tilt
Osc 2: Pulse 25%
Ring 15–25%
FM 10–15%
Env 1 → FM z szybkim atakiem i spadkiem
HP lub BP
Rezonans wysoki
Spread średni

Ostry, przenikliwy lead do industrialu i sci-fi.

4. Fib Flute
Osc 1: Sine
Osc 2: Triangle, bardzo cicho
Osc 3: Golden Pluck z wysokim tiltem
Stretch minimalny
Filter LP
Amp z lekkim atakiem 50–150 ms
LFO sine cutoff bardzo delikatnie
Delay 10%

Nie będzie prawdziwym fletem, ale organicznym, miękkim instrumentem dętym.

5. Glass Needle
Osc 1: Sine
Osc 2: Golden Pluck
Stretch Golden 0.15–0.25
FM 5–10%
HP/BP
Amp krótki attack, średni sustain
Spread 70%
Delay krótki

Cienki, szklany lead z czytelną nutą.

6. Lucas Prophet
Osc 1: Saw
Osc 2: Lucas Comb
Osc 3: Sine
Stretch Osc 2 około 0.1–0.2
LP
Env 1 → cutoff
Unison 25%
Spread 35%

Brzmienie klasycznego polysyntha z pustymi dziurami w widmie.

7. Golden Laser
Osc 1: Sine lub Triangle
FM 30–60%
Env 1 → Pitch, gwałtowny spadek 24 półtonów
Env 2 → FM, szybki decay
Amp bardzo krótki
Delay 20–40%

Do gry jako lead albo efekt.

8. Phi Choir Lead
Drawbar + Even + Triangle
Golden Detune mały
Unison 40%
Spread 80%
LP lekko przycięty
Amp attack 200–400 ms
Delay 20%

Nie realistyczny chór, tylko ludzko-syntetyczny głos.

PADS

Tu synth powinien mieć swoją najładniejszą, najbardziej „premium” stronę.

1. Phyllotaxis Bloom
Osc 1: Saw
Osc 2: Drawbar
Osc 3: Golden Pluck
Stretch wszystkich prawie 0
Golden Detune delikatny
Unison 50–70%
Spread 80–100%
Tilt około 1.6–2.0
Amp attack 1–2 s, release 3–6 s
Env 1 → Tilt 1: powolne rozjaśnienie
Env 2 → Stretch 3: powolny ruch 0 → 0.15
LFO sine + Drift → cutoff
Golden Delay 30–45%

To powinien być jeden z głównych presetów demo.

2. Golden Horizon
Trzy Saw/Triangle
Golden Int: Osc 2 +1, Osc 3 −1
Osc 2 i 3 cicho
LP
Bardzo wolny Amp
Unison 30%
Spread 70%
Delay długi

Powstaje coś pomiędzy akordem, dronem i jednym dźwiękiem. Bardzo charakterystyczne dla instrumentu.

3. Fib Cloud
Osc 1: Fib Word
Osc 2: Fib Comb
Osc 3: Sine
Stretch Fibonacci 0.1–0.25
Tilt 1.8–2.3
Spread 100%
Env 1 → Stretch 1, wolno
Env 2 → Tilt 2, wolno w przeciwną stronę
LFO Golden S&H, bardzo wolno, Drift 50%
Delay 40%

Chmura partiali, która stale reorganizuje barwę.

4. Silver Cathedral
Drawbar jako podstawa
Osc 2: Triangle w Silver z małym stretchem
Osc 3: Even
Unison 30%
Spread 90%
Filter LP
Amp attack 500 ms, bardzo długi release
Delay w sync, długi czas

Organowo-metaliczna przestrzeń.

5. Analog Drift
Saw + Saw + Triangle
Golden Detune
Niewielkie detune klasyczne
Unison 50%
Stretch bardzo niski
LP
LFO sine cutoff
Golden Drift 70–100%
Spread 60%

Najbardziej konwencjonalny analogowy pad, ale bez jawnej pętli modulacji.

6. Inharmonic Sunrise
Początek nuty: stretch wysoko
Ogon: stretch wraca do 0

Praktycznie:

Osc 1 Saw
Osc 2 Golden Pluck
Env 1 → Stretch 1, obwiednia zaczyna wysoko i schodzi
Env 2 → Stretch 2, podobnie, ale wolniej
Env 3 → FM, metaliczny początek
Amp powolny attack może osłabić transient albo szybki attack go zachować
LP powoli otwarty
Spread 80%

Pad, który zaczyna się jak niestabilna materia i krystalizuje w tonalny akord.

7. Harmonic Dissolution

Odwrotność poprzedniego:

start harmoniczny,
stretch narasta w czasie,
tilt się rozjaśnia,
dźwięk rozpada się na metaliczne partiale.

Świetny do przejść i napięcia filmowego.

8. Dark Fib Choir
Drawbar + Fib Word + Even
Tilt ciemny, około 2
LP
BP domieszki nie masz równolegle, więc wybierz LP z rezonansem
Unison duży
Spread duży
Golden S&H bardzo wolno
Delay 30–50%
9. Non-Repeating Air
Sine/Triangle/Even
mało fundamentu, więcej wysokich partiali przez niski tilt
HP
Golden S&H
Drift 100%
Spread 100%
bardzo długi attack/release

Tekstura bardziej niż klasyczny pad.

PLUCKS
1. Golden String
Golden Pluck
stretch 0
tilt około 1.3–1.7
Amp φ-casc
LP lekko przymknięty
Env 1 → Tilt: szybkie przyciemnienie
Spread 30–50%
Delay 15%

Podstawowy „instrument firmowy”.

2. Phi Koto
Golden Pluck
Osc 2: Triangle, Golden Int +1, cicho
Golden Stiff stretch 0.1–0.2
FM krótki transient
Amp szybki, decay 300–800 ms
Delay sync
3. Glass Kalimba
Osc 1: Golden Pluck
Osc 2: Sine
Golden stretch 0.15
FM 10%
Env 1 → FM, szybki decay
HP/LP zależnie od rejestru
Spread 70%
Delay 20%
4. Fib Marimba
Osc 1: Triangle
Osc 2: Fib Comb
Tilt ciemny
Stretch Fibonacci mały
Amp bardzo krótki
Env 1 → Pitch minimalnie w dół, bez quant
LP
Mały delay lub brak
5. Bronze Dulcimer
Golden Pluck + Saw
Bronze stretch 0.1–0.2
Ring 5–10%
Amp φ-casc
Spread 60%
Golden Delay
6. Stiff Pianoid
Osc 1: Golden Stiff, Triangle lub Saw
Osc 2: Sine dla fundamentu
Osc 3: Golden Pluck dla uderzenia
Env 1 → Tilt 3, bardzo szybki spadek
Env 2 → Stretch 1, delikatny ruch
Amp: szybki atak, długi nieliniowy decay, brak sustain
Spread umiarkowany

To może być jeden z najlepszych „nieistniejących fortepianów”.

7. Rain Drop
Sine + Golden Pluck
Golden stretch 0.2–0.4
wysoki pitch
Amp 50–300 ms
Delay długi, feedback umiarkowany
Arp Random φ albo Fib Walk, wolno
Velocity φ wysoko

Preset powinien działać jako samogrająca tekstura kropli.

8. Wooden Fibonacci
Triangle
Fib Comb
brak FM albo śladowy
LP z niewielką rezonancją
krótka obwiednia
spread niski
delay wyłączony

Suchy, drewniany mallet.

BELLS, MALLETS I METALE

To dałbym jako oddzielną kategorię, nie wrzucał do plucków.

1. Pure Golden Bell
Osc 1: Sine
Stretch Golden 70–100%
Tilt 1.3–1.8
FM 10–20%
Amp: szybki attack, długi wygasający release
Filter prawie otwarty
Spread 60%
Delay 20–30%
2. Silver Temple Bell
Sine lub Triangle
Silver stretch 50–80%
Ring 10–25%
FM krótki transient
Długi decay
HP lekko podniesiony

Silver powinien być ostrzejszy i bardziej agresywny od Golden.

3. Bronze Gong
Osc 1: Triangle, Bronze stretch 60–100%
Osc 2: Sine z Golden Int −1
Ring 30–50%
FM 15%
Amp bardzo długi decay
Env 1 → FM, początkowo wysoko, później 0
Env 2 → Stretch 1, powolne opadanie
Spread 100%
Delay niewielki
4. Lucas Chime
Lucas Comb
Lucas stretch 30–50%
wysoki rejestr
krótki pluck
delay 30%
spread 100%

Mało fundamentu, dużo przestrzeni między partialami.

5. Golden Shift Bar
Golden Shift wysoko
Triangle
Tilt około 1.5
Amp krótki
Ring niewielki
BP
Brak unisona

Równe przesunięcia partiali powinny dać klangor podobny do prętów, rur i metalowych sztab.

6. Fib Music Box
Golden Pluck albo Sine
Fib Comb
stretch Fibonacci niewielki
mało basu
Amp krótki
Golden Delay w sync
Arp Fibonacci 5 lub 8 kroków
7. Crystal Pendulum
Golden stretch
długi, ale cichy decay
LFO Golden S&H → cutoff
Drift
Delay wysoki feedback
Arp Random φ wolno
8. Three-Metal Morph
Osc 1: Golden
Osc 2: Silver
Osc 3: Bronze
każdy na innym poziomie stretch
obwiednie na Stretch 1, 2 i 3
oscylatory wymieniają się w czasie przez różne obwiednie
Ring 15%

Jeden dźwięk przechodzi przez trzy „materiały”.

KEYS I ORGANY
1. Phi Drawbar Organ
Wszystkie oscylatory Drawbar
różne Golden Int lub detune
stretch 0
Tilt około 1.3–1.6
Amp natychmiastowy, sustain pełny, krótki release
Unison 10–20%
Spread 50–70%
LP prawie otwarty
2. Detuned Golden Organ
Drawbar
Golden Detune stretch 20–40%
Unison 30%
Spread 80%
lekki Golden Delay

Organ z nieregularnym chorusowym ruchem bez zwykłego LFO pitch.

3. Fib Register Organ
Osc 1 Drawbar
Osc 2 Fib Comb
Osc 3 Lucas Comb
mix zależnie od rejestru
Tilt ciemny
Spread szeroki
Amp organowy
4. Even Electric Piano
Osc 1 Even
Osc 2 Sine
Osc 3 Golden Pluck
krótki FM transient
Amp podobny do elektrycznego pianina
LP
lekkie stereo
5. Golden Tine EP
Sine + Golden Pluck
Golden stretch 0.1
FM 10–20%
Env → FM szybko w dół
Amp dłuższy decay
Delay 10–20%
6. Fib Clav
Pulse 25%
Fib Word albo Fib Comb
krótka Amp
Filter BP/HP
Env → cutoff
spread niski
gate opcjonalnie
7. Alien Harpsichord
Golden Pluck + Pulse 25%
Golden Shift 0.1–0.2
bez sustain
jasny tilt
niewielki Ring
delay krótki
SYNTH STRINGS I BRASS

Nie próbowałbym udawać orkiestry. Nazwałbym kategorię Synthetic Ensemble.

1. Golden Strings
Saw + Saw + Triangle
Golden Detune
Unison 50%
Spread 100%
Amp attack 300–700 ms
LP z Env → cutoff
Drift wolny
Delay 20%
2. Fib Chamber
Saw + Fib Word + Even
mniejszy spread niż w padzie
Amp średni attack
delikatny stretch
ciemny tilt
krótki release
3. Phyllo Ensemble
Oscylatory harmoniczne
Spread 100%
Unison tylko 20–30%
różne detune
dół centralny, góra szeroka
brak mocnego delayu

To ma demonstrować samo stereo partiali.

4. Golden Brass
Saw + Square
Tilt dość ciemny
Amp attack 50–150 ms
Env 1 → cutoff z szybkim wzrostem
Env 2 → Tilt 1: chwilowe rozjaśnienie
Lekki detune i unison
Spread 30%
5. Bronze Horn
Saw + Triangle
Bronze stretch 0.05–0.1
Ring 3–8%
LP z rezonansem
Amp jak brass
Golden Int Osc 2 minimalnie lub +1 bardzo cicho

Brass z metalicznym środkiem.

DRONES I ATMOSFERY
1. Infinite Golden Machine
Osc 1 Golden
Osc 2 Silver
Osc 3 Lucas
każdy ma inny stretch
bardzo ciemny tilt
Env 1–3 sterują trzema stretchami przez kilkanaście sekund
LFO Golden S&H + Drift
Spread 100%
Delay 40–60%
Amp pełny sustain
2. Phyllotaxis Space
harmoniczne waveformy
bardzo duży spread
Unison duży
wolny drift
tilt modulowany
mało fundamentu przez mix/HP
długi delay
3. Fib Reactor
Fib Word + Fib Comb + Lucas Comb
Ring 20%
FM 15%
BP
Gate Fibonacci bardzo płytki
Arp wyłączony
obwiednie bardzo długie
4. Dark Matter
Sine + Even
sub f/φ dużo
oscylatory bardzo ciemne
mały Golden stretch
LP nisko
LFO Golden S&H minimalnie
delay wysoki feedback
5. Metal Forest
kilka metalicznych oscylatorów
długie decay’e
Arp Random φ wolno
WORD włączony
Velocity φ
Golden Delay
Gate płytki

Samogrający las rezonujących obiektów.

6. Harmonic Erosion
start czysty Drawbar/Saw
Env 1 → Stretch
Env 2 → Tilt
Env 3 → FM
wszystkie trzy powoli zwiększają chaos
długa Amp
7. Return to Order

Dokładna odwrotność: zaczyna jako hałas metalicznych partiali i przez kilka sekund zbiera się w czysty harmoniczny ton.

To świetny preset demonstracyjny, bo pokazuje wprost, po co jest stretch.

SEQUENCES / GENERATIVE

Tutaj preset musi zapisywać nie tylko barwę, ale też arp, gate, tempo-sync, word, velocity i delay.

1. Fib Walk Plucks
brzmienie: krótki Golden Pluck
Arp: Fib Walk
długość 8 lub 13
WORD on
Velocity φ 40–60%
Gate 13, depth delikatny
Delay sync
2. Weyl Music Box
music-box barwa
Arp Random φ
8 albo 13 kroków
Velocity φ 70%
WORD on
delay 30–40%
3. Golden Intervals
trzy oscylatory z Golden Int −1, 0, +1
Arp Up albo Fib Walk
gate 21
umiarkowany delay

Sekwencja składająca się z Fibonacciego w melodii i φ w harmonii.

4. Non-Repeating Pulse
krótki pluck
Gate 34 kroki
ręcznie odwrócone kilka kroków
Arp Random φ
WORD on
rate 1/16
mały delay
5. Fib Bass Computer
krótki, suchy bas
Arp Fib Walk
5 lub 8 kroków
Gate 13
Velocity φ nisko
cutoff modulowany Golden S&H
6. Spiral Rain
wysoki glass pluck
Random φ
WORD
wolne tempo, np. 1/8
długi delay
spread 100%
7. Golden Trance Gate
szeroki pad
gate 21 lub 34
depth 70–90%
arp wyłączony
delay sync
ręcznie odwrócone kroki, żeby preset miał własny rytm
8. Double Fibonacci
arp WORD steruje długościami,
gate Fibonacci steruje głośnością,
długości wzorów różne, np. arp 13, gate 21,
dzięki temu ich wspólna relacja długo się nie powtarza.

To powinien być jeden z presetów pokazowych.

FX
1. Golden Fall
Env Pitch z 833¢ Quant
kilka schodków w dół
FM malejące
Golden stretch rosnący
długi delay

Brzmi jak spadający mechanizm przeskakujący przez złote interwały.

2. Metal Impact
Ring wysoko
Bronze/Silver stretch
Amp: natychmiastowy atak, długi decay
FM transient
niski pitch
spread 100%
3. Phi Teleport
szybka schodkowa obwiednia Pitch 833¢
HP
Golden Delay
Gate bardzo szybki
krótka Amp
4. Machine Awakening
długi Amp attack
Env 1 → FM narastające
Env 2 → Stretch narastające
Env 3 → Tilt rozjaśniające
Golden S&H cutoff
delay zwiększony statycznie
Presety, które powinny być wizytówką FiSynth


BS Phi Foundation
BS Fib Reese
LD 833 Hero
LD Spiral Screamer
PD Phyllotaxis Bloom
PD Inharmonic Sunrise
PD Harmonic Dissolution
PL Golden String
KY Stiff Pianoid
BL Pure Golden Bell
BL Bronze Gong
OR Phi Drawbar Organ
DR Infinite Golden Machine
SQ Fib Walk Plucks
SQ Double Fibonacci
FX Golden Fall

dodatkowko
BS Phi Grinder
BS Golden Neuro
BS Folded Sub
BS Acid Furnace
LD Phi Shredder
LD Folded Glass
PD Warm Phi Tape
PD Shoegaze Phi
PD Burning Horizon
KY Golden Wurlitzer
PL Fractal Harp
RV Golden Cathedral
RV Frozen Glass Hall
DI Broken Golden Piano
SQ Wavefold Computer
SQ Acid Word

To jest zestaw, po którym użytkownik od razu rozumie:

φ wpływa na wysokość,
φ wpływa na widmo,
φ wpływa na stereo,
φ wpływa na czas,
φ wpływa na rytm,
Fibonacci nie jest tylko grafiką w GUI.
Jak nie zrobić banku, w którym wszystko brzmi tak samo

Największe zagrożenie przy tym silniku: każdy preset dostanie:

Golden stretch,
dużo FM,
dużo unisona,
spread 100%,
delay 50%,

i po 20 presetach wszystko będzie jedną wielką metaliczną mgłą.

Dlatego trzymaj się reguły:

Preset powinien mieć jednego głównego bohatera

Na przykład:

preset od Golden Stretch: mało Ring i FM,
preset od Ringu: stretch niewielki,
preset od Phyllotaxis: oscylatory raczej harmoniczne,
preset od FM: prosty Sine/Triangle,
preset od Fib Word: inne funkcje oszczędnie,
preset od 833¢ Pitch: barwa prosta, żeby było słychać interwały.
Nie każdy preset musi być szeroki
basy: spread 0–25%,
leady: 15–50%,
plucki: 20–70%,
pady i drony: 60–100%,
metale: zależnie od rozmiaru obiektu.
Zachowaj fundament tonalny

Przy mocnym stretchu dobrze zostawić jeden oscylator:

Sine,
Triangle,
Drawbar ze stretchem 0,
albo Sub f/φ.

Wtedy słuchacz nadal wie, jaką nutę zagrał.

Najważniejszy pomysł na presety: morfologia w czasie

Największa przewaga FiSynth nie polega na statycznym ustawieniu Golden stretch = 0.63.

Najciekawsze presety będą robiły:

atak: inharmoniczny
korpus: harmoniczny
ogon: rozciągnięty i szeroki

albo:

atak: jasny i metaliczny
korpus: ciemny, tonalny
release: ponownie rozsypany przez FM i stretch

Masz trzy obwiednie modulacyjne, więc świetny standard konstrukcji to:

Env 1 → Stretch,
Env 2 → Tilt,
Env 3 → FM.

To jest odpowiednik modelowania:

materiału,
jasności,
energii uderzenia.

Właśnie na tej trójce budowałbym większość instrumentów akustyczno-futurystycznych.

FiSynth ma najbardziej własną tożsamość jako generator nieistniejących instrumentów, organicznych metali, transformujących padów i aperiodycznych sekwencji. Nie sprzedawałbym go głównie jako „synth do basów i leadów”, bo tym konkurujesz z tysiącem pluginów. Sprzedawałbym go jako maszynę do harmonii i rytmu opartego na strukturach φ/Fibonacci, które realnie siedzą w DSP, a nie tylko w marketingowej nazwie.

DISTORTED BASSES
1. BS Phi Grinder

Ciężki, industrialny bas.

Źródło

Osc 1: Saw
Osc 2: Pulse 25%
Osc 3: Sine jako fundament
Osc 1 stretch Bronze: 10–18%
Osc 2 stretch Golden Shift: 5–10%
Ring: 10–20%
FM: 5–10%
Sub f/φ: 20–30%
Spread: 10–20%

Filtr

LP lub BP
cutoff raczej nisko
resonance 20–35%

Efekty

Saturation przed distortion: lekko
Distortion: Drive 45–65%
Tone lekko ciemniej
Mix 60–80%
Waveshaper: miękka asymetryczna krzywa, 10–20%

Modulacja

Env 1 → cutoff
Env 2 → FM, krótki metaliczny atak
Env 3 → Tilt 1, szybkie przyciemnienie

Sine i sub utrzymują wagę, a Bronze i Pulse generują środek do zniszczenia.

2. BS Golden Neuro

Najbliżej neurobasu, jakie naturalnie zrobi ten silnik.

Osc 1: Saw
Osc 2: Even
Osc 3: Fib Comb
Golden Detune 20–30%
Unison 35–50%
Spread 35–50%
Ring 20–30%
FM 20–35%

Filtr

BP z dość wysokim rezonansem
LFO triangle → cutoff, 1/8 albo 1/16
Golden Drift 30%

Efekty

Waveshaper mocno, najlepiej krzywa typu S / fold
Distortion 35–50%
Saturation 20%
Reverb prawie zero albo wyłączony

Modulacja

Env 1 → Stretch 3
Env 2 → FM
Env 3 → resonance

Najważniejsze: nie przesterowuj suba tak mocno jak środka, jeżeli efekt ma crossover lub filtr wejściowy.

3. BS Broken Speaker

Brudny, suchy, mały głośnik.

Osc 1: Square
Osc 2: Drawbar
Osc 3: Noise bardzo cicho
stretch 0
Tilt jasny
Filter BP, około 250 Hz–3 kHz
Spread 0–10%

Efekty

Hard distortion wysoko
Waveshaper clipping
Saturation po distortion
Reverb: bardzo krótki room, 3–7%

Celowo bez pełnego dołu i góry. Brzmi jak przeciążony radiowęzeł albo automat przemysłowy.

4. BS Bronze Chainsaw
Osc 1: Saw
Osc 2: Saw, Golden Int +1
Osc 3: Sine
Bronze stretch Osc 1: 15%
Silver stretch Osc 2: 8%
Ring: 15%
FM: 8%
LP dość otwarty

Efekty

Saturation 25%
Waveshaper fold 20–35%
Distortion 50%
Reverb wyłączony

Krótka obwiednia FM daje zgrzyt przy uderzeniu, ale sustain pozostaje grywalny.

5. BS Acid Furnace
Saw
LP z bardzo wysokim rezonansem
Env → cutoff
krótki decay
Gate 13 kroków
LFO cutoff lekko

Efekty

Saturation przed filtrem, jeśli architektura pozwala
Distortion po filtrze: 30–60%
Waveshaper asymetryczny
krótki spring/room reverb 5–10%

To jest preset, którego wcześniej brakowało do pełnoprawnego acidu.

6. BS Folded Sub
Osc 1: Sine
Osc 2: Sine, Golden Int +1 bardzo cicho
Sub wysoko
FM 5–15%
brak unisona
mono

Efekty

Waveshaper typu wavefold
Drive powoli zwiększany do momentu pojawienia się harmonicznych
Saturation delikatna
LP po waveshaperze, jeśli kolejność efektów na to pozwala

Czysty sinus po foldingu staje się bogatym basem bez potrzeby używania Saw.

DISTORTED LEADS
7. LD Phi Shredder
Osc 1: Saw
Osc 2: Pulse
Osc 3: Triangle
Golden Detune 10%
Unison 40%
Spread 35%
Golden Int Osc 2: +1
FM 15%

Efekty

Saturation 15%
Distortion 35–45%
Waveshaper łagodny
Reverb plate/hall 15%
Delay 15–20%

Lead gitarowo-syntezatorowy. Długi sustain, ale szybki release.

8. LD Screaming Spiral
Saw z jasnym tiltem
Golden stretch 15–25%
Ring 20%
FM 25–40%
HP albo BP
wysoka rezonancja

Efekty

Waveshaper fold 30%
Distortion 25%
Reverb plate 20%
Delay ping-pong 25%

Env → FM powinien chwilowo wbijać brzmienie w jeszcze bardziej metaliczny obszar.

9. LD Burned 833
Osc 1: Square
Osc 2: Saw, Golden Int +1
Osc 3: Saw, Golden Int −1
Env → Pitch
Quant 833¢ włączony

Efekty

Distortion 40%
Saturation 25%
Reverb 15%
Delay 30%

Schodkowe przeskoki wysokości po przesterze zaczynają przypominać syntetyczne riffy gitarowe.

10. LD Folded Glass
Sine + Golden Pluck
Golden stretch 20%
FM 10%
Spread 70%

Efekty

Waveshaper wavefold 30–50%
bez ciężkiego distortion
Reverb duży, jasny, 25–40%
Delay 15%

Glass bez waveshapera jest czysty. Po foldingu pojawiają się ostre, lśniące harmoniczne.

11. LD Radiation Horn
Saw + Drawbar
Bronze stretch 5–10%
LP z rezonansem
Amp attack 50–100 ms
Env → cutoff
Env → Tilt

Efekty

Saturation 35–50%
distortion delikatnie
waveshaper asymetryczny
room reverb 10%

Coś pomiędzy syntetycznym brass, syreną i przesterowanym instrumentem dętym.

SATURATED ANALOG

Tu saturation nie służy do niszczenia. Ma:

zagęścić średnicę,
spłaszczyć transienty,
skleić Golden Unison,
sprawić, że partiale nie brzmią zbyt laboratoryjnie.
12. PD Warm Phi Tape
Saw + Triangle + Drawbar
Stretch 0
Golden Detune 8–15%
Unison 35%
Spread 60%
Tilt około φ
LP
wolny LFO z Drift

Efekty

Saturation 25–40%
Waveshaper wyłączony
distortion 0–5%
Reverb hall 25–35%
Delay 10–15%

Podstawowy ciepły pad pokazujący, że FiSynth potrafi też być miękki.

13. KY Saturated Drawbars
trzy Drawbary z innymi proporcjami mixu
mały Golden Detune
Unison 10–20%
Spread 45%
pełny sustain

Efekty

Saturation 35%
delikatny soft clipping
krótki room 8–12%

Saturation nadaje organom masę i kompresuje sumę rejestrów.

14. BS Warm Circuit
Square + Triangle
Sub 20%
stretch 0
Tilt ciemny
LP
spread 0

Efekty

Saturation 30–45%
bardzo delikatny waveshaper
distortion wyłączony
reverb wyłączony

Prosty analogowy bas, potrzebny jako kontrast dla całej matematycznej egzotyki.

15. PD Melted Fibonacci
Fib Word + Even + Triangle
stretch Fibonacci 5–12%
Unison 40%
Spread 80%
wolne modulacje tiltu i stretcha

Efekty

Saturation wysoko, ale miękko
Reverb hall 40%
Delay 20%
distortion 5%

Saturation wygładza dziury Fib Word i skleja je w jedną organiczną powierzchnię.

16. KY Golden Wurlitzer
Osc 1: Even
Osc 2: Sine
Osc 3: Golden Pluck
krótki FM transient
Amp jak electric piano

Efekty

Saturation 30%
asymetryczny waveshaper 10–15%
krótki room 10%
delay 5%

Przy mocniejszym velocity się nie zmieni, ale możesz zrobić presetową wersję „soft” i „bark”.

WAVESHAPED / DIGITAL
17. BS Phi Wavefolder
czysty Sine jako Osc 1
Triangle Osc 2
Osc 3 wyłączony
Golden Int Osc 2 +1
Ring 5%
FM 5%

Efekty

Waveshaper fold 50–80%
saturation po nim 15%
LP przycina najwyższy piach
Reverb wyłączony

Tu waveshaper jest właściwie kolejnym typem oscylatora.

18. PL Fractal Harp
Golden Pluck
Fib Comb
jasny Tilt
krótka φ-casc
Spread 70%

Efekty

waveshaper z miękką krzywą, 15–25%
saturation 10%
duży jasny reverb 30–45%
delay złoty 20%

Wavefolder dodaje wyższe harmoniczne do uderzenia, ale reverb zamienia je w lśniący ogon.

19. KY Digital Clav Fold
Pulse 25%
Fib Word
HP/BP
krótka Amp
spread 20%

Efekty

waveshaper hard/fold 30–50%
distortion 15%
mały room
bez dużego delayu

Suchy, kanciasty instrument funkowo-industrialny.

20. LD Binary Fire
Square + Fib Comb
Golden Shift 10%
FM 20%
Env → Pitch z 833¢ Quant
Amp krótki

Efekty

waveshaper mocny
distortion średni
reverb krótki
delay 1/16

Brzmienie pod sekwencje komputerowe, cyberpunk i arcade.

21. PD Folded Choir
Drawbar + Even + Sine
Unison 40%
Spread 100%
długi Amp
LP lekko zamknięty

Efekty

delikatny fold 10–20%
saturation 20%
duży hall 45–60%
Golden Delay 20%

Fold ma tylko dodać formantopodobnych wyższych harmonicznych.

22. FX Polygon Collapse
Sine
Env 1 → FM narastająco
Env 2 → Stretch Golden narastająco
Env 3 → Pitch schodkami
HP

Efekty

waveshaper rozerwany/mocny
distortion 40%
ogromny reverb
feedback delay wysoki

Dźwięk zaczyna jako sinus, a kończy jako rozrywająca się cyfrowa konstrukcja.

REVERB INSTRUMENTS

Reverb powinien dostać osobną kategorię presetów, bo może całkowicie zmienić funkcję patcha.

23. RV Golden Cathedral
Drawbar + Triangle
Silver stretch minimalny
Unison 30%
Spread 100%
długi Amp release

Reverb

Size bardzo duży
Decay 7–15 s
Damping umiarkowany
Mix 45–60%
pre-delay, jeśli masz: 20–50 ms

Pozostałe

saturation 10–15%
delay 10%

Organowo-metaliczna katedra.

24. RV Infinite Phi Space
Sine + Golden Pluck + Fib Word
bardzo wolny attack
Env → Stretch
Env → Tilt
Golden S&H cutoff bardzo wolno
Spread 100%

Reverb

maksymalny size
bardzo długi decay
ciemne damping
Mix 60–75%

Efekty

saturation przed reverbem 10%
brak distortion

Brzmienie ma być bardziej przestrzenią niż nutą.

25. RV Bronze Cavern
Bronze Gong jako źródło
niski rejestr
Ring 30%
długi Amp decay

Efekty

saturation 20%
distortion 10%
reverb bardzo ciemny, duży, 50%
delay 10%

Wielki metalowy obiekt odbijający się w jaskini.

26. RV Frozen Glass Hall
Sine + Golden Pluck
Golden stretch 20–30%
FM transient
HP
Spread 100%

Reverb

jasny hall
duży size
długi decay
Mix 45–55%
mały damping

Waveshaper

lekki fold przed reverbem

Zimna, krystaliczna przestrzeń.

27. RV Dark Matter Chamber
Sine + Even
Sub f/φ wysoko
LP bardzo nisko
Golden S&H delikatnie
Spread 30–50%

Reverb

duży, bardzo ciemny
Mix 35–50%
długi decay

Saturation

20–30%, żeby niskie składowe wygenerowały czytelny środek

Dobry cinematic drone.

28. RV Phyllo Choir Heaven
trzy Drawbary
Golden Detune
Unison 50%
Spread 100%
Amp attack około 1 s

Efekty

saturation 15%
waveshaper łagodny 10%
reverb 50–65%
Golden Delay 15–20%

Szeroki, bardzo muzyczny pad wokalno-organowy.

29. RV Reversed Universe

Nie masz reverse reverbu, ale można oszukać percepcję.

Amp bardzo powolny attack
Env → Tilt: od ciemnego do jasnego
Env → Stretch: od 0 do Golden
Env → FM: powoli rośnie
długi release

Reverb

duży wet
długi decay
mały pre-delay

Dźwięk „zasysa się” do nuty i rozpada po puszczeniu.

DESTROYED INSTRUMENTS

To moim zdaniem będzie zajebista osobna kategoria fabryczna.

30. DI Broken Golden Piano
bazowo Stiff Pianoid
Golden Stiff 15–25%
Golden Pluck jako transient
losowości nie masz per nutę, więc LFO Golden S&H → cutoff wolno

Efekty

asymetryczny waveshaper 15%
saturation 30%
distortion 10–20%
mały, ciemny room 15%

Brzmi jak stare elektryczno-mechaniczne pianino z uszkodzonym przetwornikiem.

31. DI Rusted Kalimba
Golden Pluck
Bronze stretch 10%
Ring 5–10%
krótka Amp

Efekty

saturation 25%
distortion 15%
waveshaper lekko
mały room 20%
32. DI Burned Music Box
Sine + Fib Comb
wysoki rejestr
Arp Random φ
WORD on
Velocity φ

Efekty

distortion 20–30%
waveshaper 15%
ciemny reverb 35%
delay 25%
33. DI Corroded Strings
Saw + Even + Fib Word
Golden Detune
Unison 50%
Spread 90%
wolny attack

Efekty

distortion 10–15%
saturation 35%
waveshaper fold 10%
hall 35%

Przester powinien być przed reverbem, żeby przestrzeń zawierała już skorodowane widmo.

34. DI Radioactive Gong
Bronze + Silver
Ring wysoko
FM transient
długi decay

Efekty

waveshaper 30%
distortion 25%
saturation 20%
wielki hall 40%
delay 15%
AMBIENT DISTORTION
35. PD Shoegaze Phi
Saw + Triangle + Drawbar
Golden Detune
Unison 60%
Spread 100%
wolny Amp
LP z Drift

Efekty

distortion 25–40%
saturation 20%
waveshaper lekko
reverb 55–70%
delay 25–35%

Kluczowe: distortion przed ogromnym reverbem. Dostajesz ścianę dźwięku, nie metaliczny synth solo.

36. PD Burning Horizon
Osc 1 harmoniczny Saw
Osc 2 Golden stretch
Osc 3 Silver stretch
Env → Stretch Osc 2
Env → Tilt Osc 3
Spread 100%

Efekty

saturation 25%
distortion 15%
reverb 50%
feedback delay 25%

Horyzont powoli zaczyna się palić w wyższych partialach.

37. DR Distorted Cosmos
Fib Word + Lucas + Golden
Ring 15%
FM 10%
wolne obwiednie
Golden S&H + Drift

Efekty

waveshaper 20%
distortion 15%
saturation 20%
reverb 60%
delay 30%
38. DR Black Sun
Sine + Even + Bronze
Sub wysoko
Tilt bardzo ciemny
LP
powolny stretch Bronze

Efekty

saturation mocna
distortion 20%
ciemny reverb 50%
waveshaper asymetryczny 10%

Powinien brzmieć jak ogromna, ciężka masa, nie jak szeroki jasny pad.

SEQUENCES Z NOWYMI EFEKTAMI
39. SQ Distorted Fib Walk
krótki Pulse/Fib Comb
Arp Fib Walk
WORD on
Gate 13 albo 21
Velocity φ 60%
LFO cutoff

Efekty

distortion 35%
saturation 20%
delay 20%
krótki room 10%
40. SQ Wavefold Computer
Sine
Golden Int Osc 2 +1
Arp Random φ
1/16
Gate 34
Velocity φ

Efekty

waveshaper fold 50%
LP trochę ścina górę
delay 25%
reverb 10%
41. SQ Cathedral Mechanism
krótki Drawbar/Golden Pluck
Arp Fibonacci Up-Down
WORD on
Gate płytki
Spread 80%

Efekty

saturation 15%
ogromny reverb 50%
delay sync 25%
42. SQ Acid Word
Saw
LP resonance wysoko
Env cutoff
Gate Fibonacci
Arp Fib Walk
WORD on

Efekty

saturation 25%
distortion 40%
krótki delay
minimalny room

