cmake_minimum_required(VERSION 3.22)
project(BlackDrum VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(BlackDrum
 COMPANY_NAME "BlackDrum"
 PLUGIN_MANUFACTURER_CODE BlDr
 PLUGIN_CODE BDrm
 FORMATS VST3 Standalone
 PRODUCT_NAME "BlackDrum"
 IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(BlackDrum)
target_sources(BlackDrum PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h)
target_compile_definitions(BlackDrum PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(BlackDrum PRIVATE juce::juce_dsp juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
