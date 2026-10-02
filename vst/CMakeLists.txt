cmake_minimum_required(VERSION 3.22)
project(BASSFORFEIT VERSION 1.0.0)

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
    set(BFF_FORMATS VST3 AU Standalone)
else()
    set(BFF_FORMATS VST3 Standalone)
endif()

juce_add_plugin(BASSFORFEIT
    COMPANY_NAME "BASSFORFEIT"
    PRODUCT_NAME "BASSFORFEIT"
    PLUGIN_MANUFACTURER_CODE Bsff
    PLUGIN_CODE Bsf1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS ${BFF_FORMATS})

target_sources(BASSFORFEIT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(BASSFORFEIT PRIVATE cxx_std_17)

target_compile_definitions(BASSFORFEIT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BASSFORFEIT
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
