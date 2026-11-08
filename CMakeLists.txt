cmake_minimum_required(VERSION 3.22)
project(MELLOLAB VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MELLOLAB
    COMPANY_NAME "TapeLab"
    PLUGIN_MANUFACTURER_CODE Tlab
    PLUGIN_CODE Mllb
    FORMATS VST3
    PRODUCT_NAME "MELLOLAB"
    VERSION 1.0.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(MELLOLAB)

target_sources(MELLOLAB PRIVATE
    Source/TapeBank.cpp
    Source/TapeSynth.cpp
    Source/MelloEngine.cpp
    Source/FxRack.cpp
    Source/Settings.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# MP3 al arrastrar sonidos propios; JUCE_PLUGINHOST_VST3 permite alojar xQUILER / FRAGATIME / XPACEVERB dentro de MELLOLAB
target_compile_definitions(MELLOLAB PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_PLUGINHOST_VST3=1)

target_compile_options(MELLOLAB PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(MELLOLAB
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
