cmake_minimum_required(VERSION 3.22)
project(TALKFORFEIT VERSION 1.0.0)

# Use a local "JUCE" folder if present, otherwise download JUCE automatically
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(TF_FORMATS VST3 AU Standalone)
else()
    set(TF_FORMATS VST3 Standalone)
endif()

juce_add_plugin(TALKFORFEIT
    COMPANY_NAME "BASSFORFEIT"
    PRODUCT_NAME "TALKFORFEIT"
    PLUGIN_MANUFACTURER_CODE Bsff
    PLUGIN_CODE Tkf1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS ${TF_FORMATS})

# Pronunciation dictionary (CMU Pronouncing Dictionary, BSD licence: see Data/CMUDICT-LICENSE.txt)
juce_add_binary_data(TalkData SOURCES Data/cmudict.txt.gz)

target_sources(TALKFORFEIT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/TalkText.cpp
    Source/TalkSpeaker.cpp)

target_compile_features(TALKFORFEIT PRIVATE cxx_std_17)

target_compile_definitions(TALKFORFEIT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TALKFORFEIT
    PRIVATE
        TalkData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
