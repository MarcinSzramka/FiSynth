// Test map MIDI (.fsmap): wczytanie bdx / Akai MPK25, round-trip zapisu,
// lista map, izolacja preset<->mapa, czyszczenie wszystkich przypisań.
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    };

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto* proc = dynamic_cast<FiSynthAudioProcessor*> (base.get());
    if (proc == nullptr) { std::printf ("FAIL  brak procesora\n"); return 1; }

    const auto dir = FiSynthAudioProcessor::getMidiMapDirectory();

    // Lista zawiera przygotowane mapy.
    const auto list = proc->getMidiMapList();
    check (list.contains ("bdx"),        "lista map zawiera 'bdx'");
    check (list.contains ("Akai MPK25"), "lista map zawiera 'Akai MPK25'");

    // bdx: mapa przeniesiona ze stanu standalone.
    check (proc->loadMidiMap (dir.getChildFile ("bdx.fsmap")), "bdx.fsmap wczytany");
    check (proc->ccForParam ("gain") == 20,          "bdx: gain = CC20");
    check (proc->ccForParam ("osc1stretch") == 3,    "bdx: osc1stretch = CC3");
    check (proc->ccForParam ("filterCutoff") == 16,  "bdx: filterCutoff = CC16");
    check (proc->currentMidiMapName == "bdx",        "bdx: nazwa aktywnej mapy");

    // Akai MPK25: fabryczne CC22-29 galek K1-K8.
    check (proc->loadMidiMap (dir.getChildFile ("Akai MPK25.fsmap")), "Akai MPK25.fsmap wczytany");
    check (proc->ccForParam ("osc1stretch") == 22,   "MPK25: osc1stretch = CC22 (K1)");
    check (proc->ccForParam ("filterCutoff") == 26,  "MPK25: filterCutoff = CC26 (K5)");
    check (proc->ccForParam ("lfoRate") == 28,       "MPK25: lfoRate = CC28 (K7)");
    check (proc->ccForParam ("gain") == 29,          "MPK25: gain = CC29 (K8)");

    // Wczytanie mapy nie ma prawa ruszyc parametrow.
    auto* gainPar = proc->apvts.getParameter ("gain");
    const float g0 = gainPar->getValue();
    proc->loadMidiMap (dir.getChildFile ("bdx.fsmap"));
    check (gainPar->getValue() == g0, "wczytanie mapy nie zmienia parametrow");

    // Round-trip zapisu.
    check (proc->saveMidiMap ("test-roundtrip"), "zapis mapy 'test-roundtrip'");
    proc->clearAllCcMappings();
    check (proc->ccForParam ("gain") == -1, "clearAll: mapa pusta");
    check (proc->currentMidiMapName.isEmpty(), "clearAll: nazwa mapy pusta");
    check (proc->loadMidiMap (dir.getChildFile ("test-roundtrip.fsmap")), "round-trip wczytany");
    check (proc->ccForParam ("gain") == 20 && proc->ccForParam ("osc1stretch") == 3,
           "round-trip: mapa wraca 1:1");

    // Mapa przezywa wczytanie presetu (preset nie zawiera mapy).
    const auto presets = proc->getPresetList();
    if (! presets.isEmpty())
    {
        proc->loadPreset (FiSynthAudioProcessor::getPresetDirectory()
                              .getChildFile (presets[0] + ".fsynth"));
        check (proc->ccForParam ("gain") == 20, "mapa przezywa wczytanie presetu");

        // Preset .fsynth nie przechodzi jako mapa (zly tag glowny).
        check (! proc->loadMidiMap (FiSynthAudioProcessor::getPresetDirectory()
                                        .getChildFile (presets[0] + ".fsynth")),
               "preset .fsynth odrzucony jako mapa");
    }

    // Stan DAW: mapa jedzie w get/setStateInformation.
    juce::MemoryBlock saved;
    proc->getStateInformation (saved);
    std::unique_ptr<juce::AudioProcessor> base2 (createPluginFilter());
    auto* proc2 = dynamic_cast<FiSynthAudioProcessor*> (base2.get());
    proc2->setStateInformation (saved.getData(), (int) saved.getSize());
    check (proc2->ccForParam ("gain") == 20, "mapa w stanie DAW (round-trip przez binarke)");

    dir.getChildFile ("test-roundtrip.fsmap").deleteFile();

    // === Mapa domyslna ===
    const auto prevDefault = FiSynthAudioProcessor::getDefaultMidiMapName();
    FiSynthAudioProcessor::setDefaultMidiMapName ("bdx");
    {
        std::unique_ptr<juce::AudioProcessor> b (createPluginFilter());
        auto* p = dynamic_cast<FiSynthAudioProcessor*> (b.get());
        check (p->ccForParam ("gain") == 20, "swieza instancja dostaje mape domyslna");
        check (p->currentMidiMapName == "bdx", "swieza instancja: nazwa mapy domyslnej");

        // Pusty MIDIMAP w stanie NIE kasuje mapy domyslnej.
        p->setStateInformation (saved.getData(), (int) saved.getSize()); // z wpisami
        check (p->ccForParam ("gain") == 20, "stan z wpisami wygrywa nad domyslna");
    }
    {
        // Stan z pustym MIDIMAP -> mapa domyslna zostaje.
        std::unique_ptr<juce::AudioProcessor> b (createPluginFilter());
        auto* p = dynamic_cast<FiSynthAudioProcessor*> (b.get());
        p->clearAllCcMappings();
        juce::MemoryBlock empty;
        p->getStateInformation (empty);              // MIDIMAP bez dzieci
        p->setStateInformation (empty.getData(), (int) empty.getSize());
        check (p->ccForParam ("gain") == 20, "pusty MIDIMAP w stanie -> wraca mapa domyslna");
    }
    FiSynthAudioProcessor::setDefaultMidiMapName ({});
    {
        std::unique_ptr<juce::AudioProcessor> b (createPluginFilter());
        auto* p = dynamic_cast<FiSynthAudioProcessor*> (b.get());
        check (p->ccForParam ("gain") == -1, "bez markera swieza instancja bez mapy");
    }
    FiSynthAudioProcessor::setDefaultMidiMapName (prevDefault);

    // === Eksport ===
    const auto exportFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("fisynth-export-test");
    proc->loadMidiMap (dir.getChildFile ("bdx.fsmap"));
    check (proc->exportMidiMap (exportFile), "eksport mapy");
    {
        std::unique_ptr<juce::AudioProcessor> b (createPluginFilter());
        auto* p = dynamic_cast<FiSynthAudioProcessor*> (b.get());
        check (p->loadMidiMap (exportFile.withFileExtension ("fsmap"))
                   && p->ccForParam ("gain") == 20,
               "eksportowana mapa wczytuje sie z powrotem (z dolepionym .fsmap)");
    }
    exportFile.withFileExtension ("fsmap").deleteFile();

    // === Pickup (soft takeover) + licznik aktywnosci ===
    proc->setPlayConfigDetails (0, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    auto* gp = proc->apvts.getParameter ("gain");

    auto sendCc = [&] (int cc, int val)
    {
        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();
        juce::MidiBuffer mb;
        mb.addEvent (juce::MidiMessage::controllerEvent (1, cc, val), 0);
        proc->processBlock (buf, mb);
        juce::Thread::sleep (25);                      // timer 60 Hz musi "dojrzec"
        juce::Timer::callPendingTimersSynchronously();
    };

    proc->loadMidiMap (dir.getChildFile ("bdx.fsmap"));   // CC20 -> gain, pickup zresetowany
    gp->setValueNotifyingHost (0.8f);

    const auto activityBefore = proc->midiEventCount.load();
    sendCc (20, 10);                                      // 0.079 — daleko, bez przeciecia
    check (std::abs (gp->getValue() - 0.8f) < 2.0e-3f, "pickup: daleki CC nie szarpie parametru");
    check (proc->midiEventCount.load() > activityBefore, "licznik aktywnosci MIDI rosnie");

    sendCc (20, 127);                                     // 0.079 -> 1.0 przecina 0.8
    check (std::abs (gp->getValue() - 1.0f) < 2.0e-3f, "pickup: przeciecie przejmuje parametr");

    sendCc (20, 64);                                      // przejety — sledzi normalnie
    check (std::abs (gp->getValue() - 64.0f / 127.0f) < 2.0e-3f, "pickup: po przejeciu sledzi");

    proc->loadMidiMap (dir.getChildFile ("bdx.fsmap"));   // reset pickup
    gp->setValueNotifyingHost (0.5f);
    sendCc (20, 62);                                      // 0.488, |d|=0.012 < epsilon
    check (std::abs (gp->getValue() - 62.0f / 127.0f) < 2.0e-3f, "pickup: epsilon przejmuje");

    // Clock (0xF8) nie liczy sie do aktywnosci.
    {
        const auto before = proc->midiEventCount.load();
        juce::AudioBuffer<float> buf (2, 512);
        buf.clear();
        juce::MidiBuffer mb;
        mb.addEvent (juce::MidiMessage::midiClock(), 0);
        proc->processBlock (buf, mb);
        check (proc->midiEventCount.load() == before, "clock nie zapala kropki aktywnosci");
    }

    std::printf (failures == 0 ? "\nWSZYSTKIE TESTY OK\n" : "\n%d TESTOW PADLO\n", failures);
    return failures;
}
