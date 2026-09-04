cmake_minimum_required(VERSION 3.22)

project(FiSynth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_subdirectory(External/JUCE)

# Wersja demo: co 20 s dźwięk płynnie wycisza się na kilka sekund, GUI
# pokazuje odliczanie. Osobny PLUGIN_CODE i nazwa, żeby demo i pełna wersja
# mogły być zainstalowane obok siebie (DAW widzi je jako różne pluginy).
option(FISYNTH_DEMO "Build the demo version (audio mutes periodically)" OFF)

if(FISYNTH_DEMO)
    set(FISYNTH_PRODUCT_NAME "FiSynth Demo")
    set(FISYNTH_PLUGIN_CODE  FiSd)
else()
    set(FISYNTH_PRODUCT_NAME "FiSynth")
    set(FISYNTH_PLUGIN_CODE  FiSy)
endif()

juce_add_plugin(FiSynth
    COMPANY_NAME              "FiAudio"
    BUNDLE_ID                 "com.fiaudio.fisynth"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_MANUFACTURER_CODE  FiAu
    PLUGIN_CODE               ${FISYNTH_PLUGIN_CODE}
    FORMATS                   VST3 Standalone
    PRODUCT_NAME              "${FISYNTH_PRODUCT_NAME}")

juce_generate_juce_header(FiSynth)

target_sources(FiSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EnvelopeEditor.cpp
    Source/ModRingOverlay.cpp
    Source/SpiralVisualizer.cpp
    Source/SpectrumAnalyzer.cpp
    Source/FibGateRing.cpp
    Source/PhylloField.cpp
    Source/AboutPanel.cpp)

target_compile_definitions(FiSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(FISYNTH_DEMO)
    target_compile_definitions(FiSynth PUBLIC FISYNTH_DEMO=1)
endif()

target_link_libraries(FiSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
