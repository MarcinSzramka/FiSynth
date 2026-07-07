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
    P ("arpOn", 1); P ("arpDiv", 2); P ("arpLen", 1);
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

    if (failures == 0)
        std::printf ("\nWSZYSTKIE PRESETY OK\nkatalog: %s\n",
                     FiSynthAudioProcessor::getPresetDirectory().getFullPathName().toRawUTF8());
    else
        std::printf ("\n%d PRESETOW PADLO\n", failures);
    return failures;
}
