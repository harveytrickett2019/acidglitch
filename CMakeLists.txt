cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically so the plugin needs no extra installers on Windows.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AcidGlitch VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ACIDGLITCH_STANDALONE "Also build a standalone app (for development)" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(ACIDGLITCH_FORMATS VST3)
if(ACIDGLITCH_STANDALONE)
    list(APPEND ACIDGLITCH_FORMATS Standalone)
endif()

juce_add_plugin(AcidGlitch
    COMPANY_NAME "AcidGlitch"
    PLUGIN_MANUFACTURER_CODE Acgl
    PLUGIN_CODE Acg1
    FORMATS ${ACIDGLITCH_FORMATS}
    PRODUCT_NAME "AcidGlitch"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AcidGlitch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(AcidGlitch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AcidGlitch
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
