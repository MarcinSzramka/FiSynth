// Test zapisu/wczytywania presetów FiSynth:
// 1) czy nowe parametry (złoty silnik) trafiają do zapisu stanu,
// 2) czy STARY preset (bez wpisów nowych parametrów) resetuje je do domyślnych.
#include <JuceHeader.h>
#include <cstring>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static juce::RangedAudioParameter* getParam (juce::AudioProcessor& p, const juce::String& id)
{
    for (auto* par : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (par))
            if (rp->paramID == id)
                return rp;
    return nullptr;
}

// Format copyXmlToBinary: magic u32 LE 0x21324356, u32 długość, XML, NUL.
static std::unique_ptr<juce::XmlElement> unwrapState (const juce::MemoryBlock& mb)
{
    auto* d = static_cast<const char*> (mb.getData());
    juce::uint32 len;
    std::memcpy (&len, d + 4, 4);
    return juce::XmlDocument::parse (juce::String::fromUTF8 (d + 8, (int) len));
}

static juce::MemoryBlock wrapState (const juce::XmlElement& xml)
{
    const auto text = xml.toString (juce::XmlElement::TextFormat().singleLine());
    juce::MemoryBlock mb;
    juce::MemoryOutputStream out (mb, false);
    out.writeInt (0x21324356);
    out.writeInt ((int) text.getNumBytesAsUTF8() + 1);
    out.write (text.toRawUTF8(), text.getNumBytesAsUTF8());
    out.writeByte (0);
    return mb;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failures;
    };

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());

    auto* dlyOn  = getParam (*proc, "dlyOn");
    auto* ring   = getParam (*proc, "ringMix");
    auto* tilt1  = getParam (*proc, "osc1tilt");
    auto* arpOn  = getParam (*proc, "arpOn");
    check (dlyOn && ring && tilt1 && arpOn, "nowe parametry zarejestrowane");

    // Podkręć złoty silnik i zapisz stan (symulacja zapisu presetu).
    dlyOn->setValueNotifyingHost (1.0f);
    ring->setValueNotifyingHost (ring->convertTo0to1 (0.7f));
    tilt1->setValueNotifyingHost (tilt1->convertTo0to1 (1.618f));
    arpOn->setValueNotifyingHost (1.0f);

    juce::MemoryBlock saved;
    proc->getStateInformation (saved);
    auto savedXml = unwrapState (saved);
    check (savedXml != nullptr, "stan parsuje się z binarki");

    int found = 0;
    for (auto* ch : savedXml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = ch->getStringAttribute ("id");
        if (id == "dlyOn" || id == "ringMix" || id == "osc1tilt" || id == "arpOn"
            || id == "fmAmt" || id == "subLevel" || id == "unison" || id == "lfoDrift"
            || id == "pitchQuant" || id == "dlyFeedback" || id == "arpDiv")
            ++found;
    }
    check (found == 11, "zapis zawiera wszystkie nowe parametry (11/11)");

    // Round-trip do świeżej instancji: wartości mają wrócić 1:1.
    std::unique_ptr<juce::AudioProcessor> proc2 (createPluginFilter());
    proc2->setStateInformation (saved.getData(), (int) saved.getSize());
    check (getParam (*proc2, "dlyOn")->getValue() > 0.5f, "round-trip: dlyOn = on");
    const float ringBack = getParam (*proc2, "ringMix")->convertFrom0to1 (
        getParam (*proc2, "ringMix")->getValue());
    check (std::abs (ringBack - 0.7f) < 0.02f, "round-trip: ringMix = 0.7");

    // Symulacja STAREGO presetu: usuń wpisy nowych parametrów z XML.
    auto oldXml = std::make_unique<juce::XmlElement> (*savedXml);
    static const char* newIds[] = { "dlyOn", "ringMix", "osc1tilt", "osc2tilt", "osc3tilt",
                                    "arpOn", "arpDiv", "arpLen", "fmAmt", "subLevel",
                                    "unison", "lfoDrift", "pitchQuant", "dlySync",
                                    "dlyTime", "dlyDiv", "dlyFeedback", "dlyMix" };
    for (int i = oldXml->getNumChildElements() - 1; i >= 0; --i)
    {
        auto* ch = oldXml->getChildElement (i);
        if (ch->hasTagName ("PARAM"))
            for (auto* id : newIds)
                if (ch->getStringAttribute ("id") == id)
                { oldXml->removeChildElement (ch, true); break; }
    }

    // Wczytaj "stary" preset na instancji z podkręconym silnikiem (proc,
    // wciąż dlyOn=1, ring=0.7): brakujące parametry MUSZĄ wrócić do domyślnych.
    const auto oldBlob = wrapState (*oldXml);
    proc->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize());

    check (getParam (*proc, "dlyOn")->getValue() < 0.5f,
           "stary preset: dlyOn wraca do OFF");
    check (getParam (*proc, "ringMix")->getValue() < 0.01f,
           "stary preset: ringMix wraca do 0");
    const float tiltBack = getParam (*proc, "osc1tilt")->convertFrom0to1 (
        getParam (*proc, "osc1tilt")->getValue());
    check (std::abs (tiltBack - 1.0f) < 0.01f, "stary preset: tilt wraca do 1.0");
    check (getParam (*proc, "arpOn")->getValue() < 0.5f,
           "stary preset: arpOn wraca do OFF");
    // Parametr OBECNY w starym presecie ma się normalnie wczytać.
    const float gain = getParam (*proc, "gain")->convertFrom0to1 (
        getParam (*proc, "gain")->getValue());
    check (std::abs (gain - 0.8f) < 0.01f, "stary preset: gain wczytany z pliku");

    std::printf (failures == 0 ? "\nWSZYSTKIE TESTY OK\n" : "\n%d TESTOW PADLO\n", failures);
    return failures;
}
