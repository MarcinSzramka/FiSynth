// Generator presetów fabrycznych: konfiguruje realny procesor i zapisuje
// przez savePreset() (ten sam serializer co GUI/DAW — pliki na pewno wczytywalne).
// Po każdym zapisie krótki render-smoke: preset musi wydawać dźwięk.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static FiSynthAudioProcessor* gProc = nullptr;

static void P (const juce::String& id, float plain)
{
    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (gProc->apvts.getParameter (id)))
        rp->setValueNotifyingHost (rp->convertTo0to1 (plain));
    else
        std::printf ("!! BRAK PARAMETRU %s\n", id.toRawUTF8());
}

// Obwiednia idx: lista punktów {czas s, poziom, krzywizna} + indeks sustain.
static void Env (int idx, std::initializer_list<EnvPoint> pts, int sustainIdx)
{
    auto& m = gProc->getEnvelopeModel (idx);
    m.points.assign (pts);
    m.sustainIndex = sustainIdx;
    m.sortAndClamp();
    gProc->commitEnvelope (idx);
}

// Render-smoke: akord C3+G3 przez ~0.7 s, RMS musi być > progu.
static bool smoke()
{
    constexpr int blk = 512;
    juce::AudioBuffer<float> buf (2, blk);
    double acc = 0.0; size_t cnt = 0;
    for (int b = 0; b < 60; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
        }
        gProc->processBlock (buf, midi);
        for (int i = 0; i < blk; ++i)
        {
            const double s = 0.5 * (buf.getSample (0, i) + buf.getSample (1, i));
            acc += s * s; ++cnt;
        }
    }
    // zdejmij nuty i wycisz ogon (delay/release), żeby nie przeciekał do kolejnego
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    for (int b = 0; b < 400; ++b)
    {
        juce::MidiBuffer m; if (b == 0) m = off;
        gProc->processBlock (buf, m);
    }
    return std::sqrt (acc / (double) cnt) > 1.0e-4;
}

static int failures = 0;
static void save (const juce::String& name)
{
    const bool ok = gProc->savePreset (name) && smoke();
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name.toRawUTF8());
    if (! ok) ++failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    gProc = dynamic_cast<FiSynthAudioProcessor*> (proc.get());
    gProc->setPlayConfigDetails (0, 2, 48000.0, 512);
    gProc->prepareToPlay (48000.0, 512);

    // ============ 01 · Stiff Piano — fortepianowy stretch, naturalny ============
    gProc->resetToInit();
    P ("gain", 0.62f);
    P ("osc1waveform", 3);  P ("osc1mix", 0.70f); P ("osc1stretch", 0.12f);
    P ("osc1stretchmode", 6); P ("osc1tilt", 1.6f);
    P ("osc2waveform", 2);  P ("osc2mix", 0.40f); P ("osc2stretch", 0.12f);
    P ("osc2stretchmode", 6); P ("osc2detune", 4); P ("osc2tilt", 1.2f);
    P ("filterCutoff", 6500); P ("filterResonance", 0.6f);
    P ("spread", 0.25f); P ("subLevel", 0.12f);
    Env (0, { {0,0,0}, {0.004f,1,0}, {0.35f,0.45f,-0.4f}, {1.4f,0.18f,-0.35f}, {3.2f,0,-0.3f} }, 4);
    save ("01 Stiff Piano");

    // ============ 02 · Golden Pluck Harp — struna z złotego podziału ============
    gProc->resetToInit();
    P ("gain", 0.65f);
    P ("osc1waveform", 9);  P ("osc1mix", 0.85f);
    P ("osc2waveform", 9);  P ("osc2mix", 0.35f); P ("osc2goldint", 3); P ("osc2detune", 3);
    P ("filterCutoff", 12000);
    P ("spread", 0.5f); P ("subLevel", 0.18f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.35f); P ("dlyMix", 0.25f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {0.5f,0.3f,-0.45f}, {2.2f,0,-0.35f} }, 3);
    save ("02 Golden Pluck Harp");

    // ============ 03 · Drawbar Organ — rejestry + leslie z driftu ============
    gProc->resetToInit();
    P ("gain", 0.6f);
    P ("osc1waveform", 7);  P ("osc1mix", 0.80f);
    P ("osc2waveform", 8);  P ("osc2mix", 0.30f); P ("osc2detune", -5);
    P ("filterCutoff", 8000); P ("filterResonance", 0.4f);
    P ("lfoRate", 5.5f); P ("lfoDepth", 0.30f); P ("lfoDrift", 0.5f);
    P ("spread", 0.45f);
    Env (0, { {0,0,0}, {0.015f,1,0}, {0.3f,0,-0.2f} }, 1);
    save ("03 Drawbar Organ");

    // ============ 04 · Silk Pad — golden octave, unison, drift ============
    gProc->resetToInit();
    P ("gain", 0.6f);
    P ("osc1waveform", 3);  P ("osc1mix", 0.60f); P ("osc1stretch", 0.22f);
    P ("osc1stretchmode", 2); P ("osc1tilt", 1.9f);
    P ("osc2waveform", 3);  P ("osc2mix", 0.55f); P ("osc2stretch", 0.22f);
    P ("osc2stretchmode", 2); P ("osc2detune", -6); P ("osc2tilt", 1.9f);
    P ("osc3waveform", 0);  P ("osc3mix", 0.30f); P ("osc3goldint", 1);
    P ("unison", 0.6f); P ("spread", 0.85f); P ("subLevel", 0.25f);
    P ("filterCutoff", 3200); P ("filterResonance", 0.5f);
    P ("lfoRate", 0.8f); P ("lfoDepth", 0.2f); P ("lfoDrift", 0.7f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 0); P ("dlyFeedback", 0.45f); P ("dlyMix", 0.28f);
    P ("env2Dest", 1); P ("env2Amt", 0.35f);
    Env (0, { {0,0,0}, {0.9f,1,0.2f}, {3.2f,0,-0.3f} }, 1);
    Env (2, { {0,0,0}, {1.5f,1,0}, {4,0,0} }, 1);
    save ("04 Silk Pad");

    // ============ 05 · Fibonacci Music Box — grzebień Fibonacciego ============
    gProc->resetToInit();
    P ("gain", 0.65f);
    P ("osc1waveform", 10); P ("osc1mix", 0.80f); P ("osc1goldint", 4); P ("osc1tilt", 0.8f);
    P ("osc2waveform", 9);  P ("osc2mix", 0.30f); P ("osc2goldint", 3);
    P ("filterCutoff", 14000);
    P ("spread", 0.6f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.30f); P ("dlyMix", 0.30f);
    Env (0, { {0,0,0}, {0.002f,1,0}, {0.9f,0,-0.45f} }, 2);
    save ("05 Fibonacci Music Box");

    // ============ 06 · Golden Bells — ring osc1×osc2 w interwale φ ============
    gProc->resetToInit();
    P ("gain", 0.62f);
    P ("osc1waveform", 3);  P ("osc1mix", 0.30f); P ("osc1stretch", 0.45f);
    P ("osc1stretchmode", 0); P ("osc1tilt", 1.3f);
    P ("osc2waveform", 3);  P ("osc2mix", 0.25f); P ("osc2stretch", 0.45f);
    P ("osc2stretchmode", 0); P ("osc2goldint", 3);
    P ("ringMix", 0.85f); P ("fmAmt", 0.12f);
    P ("filterCutoff", 9000); P ("filterResonance", 0.8f);
    P ("spread", 0.55f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.40f); P ("dlyMix", 0.30f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {1.2f,0.35f,-0.5f}, {4.5f,0,-0.4f} }, 3);
    save ("06 Golden Bells");

    // ============ 07 · Metal Shimmer — FM pod obwiednią, Silver, S&H ============
    gProc->resetToInit();
    P ("gain", 0.58f);
    P ("osc1waveform", 3);  P ("osc1mix", 0.60f); P ("osc1stretch", 0.60f);
    P ("osc1stretchmode", 4); P ("osc1tilt", 1.1f);
    P ("osc2waveform", 4);  P ("osc2mix", 0.35f); P ("osc2stretch", 0.50f);
    P ("osc2stretchmode", 4); P ("osc2goldint", 3);
    P ("fmAmt", 0.30f);
    P ("env2Dest", 8); P ("env2Amt", 0.65f);
    P ("lfoShape", 3); P ("lfoSync", 1); P ("lfoRateDiv", 4); P ("lfoDepth", 0.35f);
    P ("unison", 0.4f); P ("spread", 0.7f);
    P ("filterCutoff", 7000); P ("filterResonance", 1.2f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.40f); P ("dlyMix", 0.28f);
    Env (0, { {0,0,0}, {0.05f,1,0}, {0.6f,0.75f,-0.3f}, {3,0,-0.3f} }, 2);
    Env (2, { {0,1,0}, {1.4f,0,-0.4f} }, 1);
    save ("07 Metal Shimmer");

    // ============ 08 · Golden Run — arpeggiator + kaskada ech ============
    gProc->resetToInit();
    P ("gain", 0.62f);
    P ("bpm", 120);
    P ("arpOn", 1); P ("arpDiv", 2); P ("arpLen", 4);
    P ("osc1waveform", 9);  P ("osc1mix", 0.80f); P ("osc1tilt", 1.2f);
    P ("osc2waveform", 3);  P ("osc2mix", 0.30f); P ("osc2stretch", 0.30f); P ("osc2stretchmode", 1);
    P ("filterCutoff", 11000);
    P ("spread", 0.5f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.50f); P ("dlyMix", 0.35f);
    Env (0, { {0,0,0}, {0.002f,1,0}, {0.35f,0,-0.4f} }, 2);
    save ("08 Golden Run");

    // ============ 09 · Fib Gate Trance — bramka 13 kroków + sweep ============
    gProc->resetToInit();
    P ("gain", 0.6f);
    P ("bpm", 126);
    P ("gateOn", 1); P ("gateDepth", 0.9f); P ("gateDiv", 2); P ("gateGen", 2);
    P ("osc1waveform", 3);  P ("osc1mix", 0.65f); P ("osc1stretch", 0.15f);
    P ("osc1stretchmode", 3); P ("osc1tilt", 1.4f);
    P ("osc2waveform", 3);  P ("osc2mix", 0.60f); P ("osc2detune", -7);
    P ("osc2stretch", 0.15f); P ("osc2stretchmode", 3);
    P ("osc3waveform", 3);  P ("osc3mix", 0.40f); P ("osc3detune", 7);
    P ("unison", 0.8f); P ("spread", 0.95f); P ("subLevel", 0.3f);
    P ("filterCutoff", 1800); P ("filterResonance", 1.6f);
    P ("env2Dest", 1); P ("env2Amt", 0.55f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 1); P ("dlyFeedback", 0.45f); P ("dlyMix", 0.30f);
    Env (0, { {0,0,0}, {0.05f,1,0}, {0.6f,0,-0.2f} }, 1);
    Env (2, { {0,0,0}, {2.5f,1,0.3f}, {4.5f,0,0} }, 1);
    save ("09 Fib Gate Trance");

    // ============ 10 · Zloty Skurwysyn — wszystko na raz, złote schodki ============
    gProc->resetToInit();
    P ("gain", 0.5f);
    P ("osc1waveform", 3);  P ("osc1mix", 0.55f); P ("osc1stretch", 0.85f);
    P ("osc1stretchmode", 5); P ("osc1tilt", 0.6f);
    P ("osc2waveform", 12); P ("osc2mix", 0.50f); P ("osc2goldint", 4);
    P ("osc2stretch", 0.70f); P ("osc2stretchmode", 7);
    P ("osc3waveform", 5);  P ("osc3mix", 0.12f);
    P ("ringMix", 0.7f); P ("fmAmt", 0.8f); P ("subLevel", 0.5f); P ("unison", 1.0f);
    P ("spread", 1.0f); P ("pitchQuant", 1);
    P ("env2Dest", 8); P ("env2Amt", 0.6f);
    P ("env3Dest", 2); P ("env3Amt", 0.55f);
    P ("lfoShape", 3); P ("lfoSync", 1); P ("lfoRateDiv", 5); P ("lfoDepth", 0.9f); P ("lfoDrift", 1.0f);
    P ("gateOn", 1); P ("gateDepth", 0.8f); P ("gateDiv", 3); P ("gateGen", 3);
    P ("filterType", 1); P ("filterCutoff", 2200); P ("filterResonance", 3.5f);
    P ("dlyOn", 1); P ("dlySync", 0); P ("dlyTime", 160); P ("dlyFeedback", 0.72f); P ("dlyMix", 0.45f);
    Env (0, { {0,0,0}, {0.01f,1,0}, {1.5f,0,-0.2f} }, 1);
    Env (2, { {0,1,0}, {1.2f,0,-0.4f} }, 1);
    Env (3, { {0,0,0}, {0.8f,1,0}, {1.6f,0,0} }, 2);
    save ("10 Zloty Skurwysyn");

    // ============ 11 · Deep Field Drone — głęboki kosmiczny dron ============
    // Even + Fib Comb w Golden Octave/Lucas, oddychający filtr (LFO z driftem φ),
    // widmo powoli rozciąga się obwiednią (Stretch 1) — pad "z teleskopu Webba".
    gProc->resetToInit();
    P ("gain", 0.6f);
    P ("osc1waveform", 8);  P ("osc1mix", 0.55f); P ("osc1stretch", 0.30f);
    P ("osc1stretchmode", 2); P ("osc1tilt", 2.2f);
    P ("osc2waveform", 10); P ("osc2mix", 0.40f); P ("osc2stretch", 0.20f);
    P ("osc2stretchmode", 7); P ("osc2goldint", 1); P ("osc2tilt", 2.4f);
    P ("osc3waveform", 0);  P ("osc3mix", 0.35f); P ("osc3goldint", 3);
    P ("subLevel", 0.45f); P ("unison", 0.85f); P ("spread", 1.0f);
    P ("filterCutoff", 1500); P ("filterResonance", 0.6f);
    P ("lfoRate", 0.2f); P ("lfoDepth", 0.30f); P ("lfoDrift", 0.9f);
    P ("env2Dest", 5); P ("env2Amt", 0.35f);
    P ("env3Dest", 1); P ("env3Amt", 0.30f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 0); P ("dlyFeedback", 0.60f); P ("dlyMix", 0.35f);
    Env (0, { {0,0,0}, {1.2f,1,0.25f}, {6.0f,0,-0.3f} }, 1);
    Env (2, { {0,0,0}, {4.0f,1,0.2f}, {8.0f,0,-0.2f} }, 1);
    Env (3, { {0,0,0}, {2.5f,1,0.3f}, {7.0f,0,0} }, 1);
    save ("11 Deep Field Drone");

    // ============ 12 · Nebula Chorale — chór mgławicy przez band-pass ============
    // Golden Detune (mikro-rozstrojony chór) + band-pass wędrujący obwiednią
    // jak powoli zmieniająca się samogłoska; iskra +2φ na trzecim oscylatorze.
    gProc->resetToInit();
    P ("gain", 0.6f);
    P ("osc1waveform", 8); P ("osc1mix", 0.55f); P ("osc1stretch", 0.50f);
    P ("osc1stretchmode", 3); P ("osc1tilt", 1.6f);
    P ("osc2waveform", 2); P ("osc2mix", 0.45f); P ("osc2stretch", 0.50f);
    P ("osc2stretchmode", 3); P ("osc2detune", 5); P ("osc2tilt", 1.8f);
    P ("osc3waveform", 0); P ("osc3mix", 0.28f); P ("osc3goldint", 4);
    P ("unison", 0.75f); P ("spread", 0.95f); P ("subLevel", 0.2f);
    P ("filterType", 1); P ("filterCutoff", 900); P ("filterResonance", 1.3f);
    P ("env2Dest", 1); P ("env2Amt", 0.50f);
    P ("lfoShape", 2); P ("lfoRate", 0.3f); P ("lfoDepth", 0.25f); P ("lfoDrift", 0.65f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 0); P ("dlyFeedback", 0.55f); P ("dlyMix", 0.40f);
    Env (0, { {0,0,0}, {0.8f,1,0.2f}, {4.5f,0,-0.3f} }, 1);
    Env (2, { {0,0,0}, {3.0f,1,0.3f}, {8.0f,0.25f,-0.2f} }, 1);
    save ("12 Nebula Chorale");

    // ============ 13 · Pulsar Beacon — radiolatarnia: arp Random φ + echo 1/8T ============
    // Melodia nigdy się nie zapętla (schodki Weyla), rytm ze słowa Fibonacciego,
    // echa triolowe krzyżują się z krokiem 1/8 — sygnał z obcej sondy.
    gProc->resetToInit();
    P ("gain", 0.62f);
    P ("bpm", 96);
    P ("arpOn", 1); P ("arpDiv", 1); P ("arpLen", 3); P ("arpMode", 4);
    P ("arpWord", 1); P ("arpVel", 0.7f);
    P ("osc1waveform", 9);  P ("osc1mix", 0.75f); P ("osc1tilt", 1.1f);
    P ("osc2waveform", 10); P ("osc2mix", 0.35f); P ("osc2goldint", 3); P ("osc2tilt", 1.3f);
    P ("osc3waveform", 0);  P ("osc3mix", 0.25f); P ("osc3goldint", 1);
    P ("subLevel", 0.3f); P ("spread", 0.8f); P ("unison", 0.3f);
    P ("filterCutoff", 5500); P ("filterResonance", 1.0f);
    P ("env2Dest", 1); P ("env2Amt", 0.40f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 4); P ("dlyFeedback", 0.65f); P ("dlyMix", 0.45f);
    Env (0, { {0,0,0}, {0.003f,1,0}, {1.0f,0,-0.45f} }, 2);
    Env (2, { {0,1,0}, {0.5f,0,-0.4f} }, 1);
    save ("13 Pulsar Beacon");

    // ============ 14 · Alien Larynx — gadający obcy: formanty + kwant 833¢ ============
    // Band-pass z wysokim Q sterowany "mową" (obwiednia wielopunktowa), FM
    // wybuchami, pitch skacze schodkami złotego interwału — glossolalia obcych.
    gProc->resetToInit();
    P ("gain", 0.55f);
    P ("osc1waveform", 12); P ("osc1mix", 0.65f); P ("osc1stretch", 0.40f);
    P ("osc1stretchmode", 8); P ("osc1tilt", 0.9f);
    P ("osc2waveform", 4);  P ("osc2mix", 0.50f); P ("osc2goldint", 1);
    P ("osc2stretch", 0.30f); P ("osc2stretchmode", 4); P ("osc2tilt", 0.8f);
    P ("subLevel", 0.35f);
    P ("ringMix", 0.6f); P ("fmAmt", 0.25f);
    P ("filterType", 1); P ("filterCutoff", 750); P ("filterResonance", 5.0f);
    P ("env1Dest", 1); P ("env1Amt", 0.55f);
    P ("env2Dest", 8); P ("env2Amt", 0.50f);
    P ("env3Dest", 2); P ("env3Amt", 0.70f); P ("pitchQuant", 1);
    P ("lfoShape", 3); P ("lfoSync", 1); P ("lfoRateDiv", 3); P ("lfoDepth", 0.5f);
    P ("dlyOn", 1); P ("dlySync", 0); P ("dlyTime", 120); P ("dlyFeedback", 0.5f); P ("dlyMix", 0.3f);
    Env (0, { {0,0,0}, {0.02f,1,0}, {0.9f,0.7f,-0.2f}, {2.4f,0,-0.3f} }, 2);
    Env (1, { {0,0.2f,0}, {0.15f,1,-0.3f}, {0.35f,0.35f,0.3f}, {0.6f,0.8f,0},
              {0.9f,0.15f,0}, {1.3f,0.55f,0} }, 5);
    Env (2, { {0,0,0}, {0.1f,1,0}, {0.25f,0.1f,0}, {0.5f,0.7f,0}, {0.85f,0,0} }, 4);
    Env (3, { {0,0.6f,0}, {0.3f,0.6f,0}, {0.35f,1,0}, {0.6f,1,0},
              {0.65f,0.3f,0}, {0.95f,0.3f,0}, {1.0f,0,0} }, 6);
    save ("14 Alien Larynx");

    // ============ 15 · Xeno Hive — rój: Silver/Bronze + gate 34 kroki 1/32 ============
    // Dwa saw rozciągnięte metalicznie (σ srebrne i brązowe), szczypta szumu,
    // aperiodyczny stutter słowa Fibonacciego; rój z czasem "składa się"
    // w harmoniczność (obwiednia cofa Stretch 2).
    gProc->resetToInit();
    P ("gain", 0.55f);
    P ("bpm", 128);
    P ("osc1waveform", 3); P ("osc1mix", 0.60f); P ("osc1stretch", 0.75f);
    P ("osc1stretchmode", 4); P ("osc1tilt", 0.8f);
    P ("osc2waveform", 3); P ("osc2mix", 0.55f); P ("osc2stretch", 0.85f);
    P ("osc2stretchmode", 5); P ("osc2detune", -5); P ("osc2tilt", 0.75f);
    P ("osc3waveform", 5); P ("osc3mix", 0.15f);
    P ("unison", 1.0f); P ("spread", 1.0f); P ("subLevel", 0.3f); P ("fmAmt", 0.3f);
    P ("filterCutoff", 1600); P ("filterResonance", 2.5f);
    P ("env2Dest", 1); P ("env2Amt", 0.60f);
    P ("env3Dest", 6); P ("env3Amt", -0.50f);
    P ("gateOn", 1); P ("gateDepth", 0.9f); P ("gateDiv", 3); P ("gateGen", 4);
    P ("lfoShape", 1); P ("lfoSync", 1); P ("lfoRateDiv", 6); P ("lfoDepth", 0.35f);
    P ("dlyOn", 1); P ("dlySync", 1); P ("dlyDiv", 2); P ("dlyFeedback", 0.5f); P ("dlyMix", 0.3f);
    Env (0, { {0,0,0}, {0.01f,1,0}, {0.8f,0,-0.25f} }, 1);
    Env (2, { {0,1,0}, {0.35f,0.15f,-0.4f}, {1.5f,0,0} }, 1);
    Env (3, { {0,0,0}, {1.4f,1,0.2f} }, 1);
    save ("15 Xeno Hive");

    // ============ 16 · Wormhole Screamer — opadający krzyk przez ring+FM ============
    // Golden Shift (klangor) × Fib Word +2φ przez ring i FM pod obwiednią,
    // pitch spada z +21 półtonów schodkami 833¢, rezonans narasta, krótki
    // metaliczny delay 75 ms — syrena z tunelu czasoprzestrzennego.
    gProc->resetToInit();
    P ("gain", 0.5f);
    P ("osc1waveform", 1);  P ("osc1mix", 0.55f); P ("osc1stretch", 0.90f);
    P ("osc1stretchmode", 8); P ("osc1tilt", 0.7f);
    P ("osc2waveform", 12); P ("osc2mix", 0.50f); P ("osc2goldint", 4);
    P ("osc2stretch", 0.60f); P ("osc2stretchmode", 0); P ("osc2tilt", 0.9f);
    P ("osc3waveform", 11); P ("osc3mix", 0.30f); P ("osc3goldint", 0);
    P ("ringMix", 0.85f); P ("fmAmt", 0.45f); P ("subLevel", 0.55f);
    P ("unison", 0.8f); P ("spread", 1.0f); P ("pitchQuant", 1);
    P ("filterType", 1); P ("filterCutoff", 1200); P ("filterResonance", 4.0f);
    P ("env1Dest", 4); P ("env1Amt", 0.80f);
    P ("env2Dest", 8); P ("env2Amt", 0.55f);
    P ("env3Dest", 2); P ("env3Amt", 0.90f);
    P ("lfoShape", 3); P ("lfoSync", 1); P ("lfoRateDiv", 4); P ("lfoDepth", 0.6f); P ("lfoDrift", 1.0f);
    P ("dlyOn", 1); P ("dlySync", 0); P ("dlyTime", 75); P ("dlyFeedback", 0.75f); P ("dlyMix", 0.35f);
    Env (0, { {0,0,0}, {0.005f,1,0}, {2.5f,0.8f,-0.2f}, {4.5f,0,-0.3f} }, 2);
    Env (1, { {0,0,0}, {1.5f,1,0.3f} }, 1);
    Env (2, { {0,1,0}, {0.6f,0.2f,-0.4f}, {1.4f,0.9f,0}, {2.4f,0,0} }, 3);
    Env (3, { {0,1,0}, {1.8f,0,-0.5f} }, 1);
    save ("16 Wormhole Screamer");

    if (failures == 0)
        std::printf ("\nWSZYSTKIE PRESETY OK\nkatalog: %s\n",
                     FiSynthAudioProcessor::getPresetDirectory().getFullPathName().toRawUTF8());
    else
        std::printf ("\n%d PRESETOW PADLO\n", failures);
    return failures;
}
