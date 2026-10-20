cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(WACKD VERSION 1.5.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Wackd
    COMPANY_NAME                "Studio Wiga"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Wiga
    PLUGIN_CODE                 Wkcf
    FORMATS                     VST3
    PRODUCT_NAME                "Wackd"
    VERSION                     1.5.4
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Modulation"
)

juce_add_binary_data(WackdData SOURCES background.jpg bars_lit.png bars_lit_amber.png mode_sw.png mode_sw_r.png krazy_sw.png krazy_sw_r.png knob_strip.png Montserrat-Bold.ttf)
set_target_properties(WackdData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(Wackd PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_definitions(Wackd PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Wackd
    PRIVATE
        WackdData
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
