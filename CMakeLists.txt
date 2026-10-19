cmake_minimum_required(VERSION 3.22)

project(DS78 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Versione minima di macOS")
endif()

option(DS78_BUILD_TESTS "Compila i test del motore e l'istantanea dell'interfaccia" OFF)
set(JUCE_DIR "" CACHE PATH "Cartella locale di JUCE (vuoto = scaricato automaticamente)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(DS78
    COMPANY_NAME "Collateral Effects"
    COMPANY_WEBSITE "https://github.com/mermau"
    BUNDLE_ID "com.collateraleffects.ds78"
    PLUGIN_MANUFACTURER_CODE Clfx
    PLUGIN_CODE Ds78
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "DS-78"
    DESCRIPTION "Emulazione del sintetizzatore Crumar DS-2"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")

juce_add_binary_data(DS78_Data SOURCES
    resources/fonts/Michroma.ttf
    resources/fonts/AlfaSlabOne.ttf)

target_sources(DS78 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(DS78 PRIVATE src)
target_compile_options(DS78 PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-float-equal>)

target_compile_definitions(DS78 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DS78
    PRIVATE
        DS78_Data
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(DS78_BUILD_TESTS)
    add_executable(engine_test tests/engine_test.cpp)
    target_include_directories(engine_test PRIVATE src)

    juce_add_console_app(gui_snapshot PRODUCT_NAME "gui_snapshot")
    target_sources(gui_snapshot PRIVATE tests/gui_snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(gui_snapshot PRIVATE src)
    target_compile_definitions(gui_snapshot PRIVATE
        JucePlugin_Name="DS-78"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(gui_snapshot PRIVATE
        DS78_Data juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
