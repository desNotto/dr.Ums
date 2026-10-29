cmake_minimum_required(VERSION 3.22)

# Статическая среда выполнения MSVC: плагину не нужен установленный VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DrUms VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Если JUCE уже лежит рядом, можно собрать без скачивания:
#   cmake -B build -DJUCE_DIR=C:/path/to/JUCE
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.16-security
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Плагин оформлен как "инструмент" (generator) с тихим стерео-выходом: так MIDI-выход
# видят практически все DAW. Если нужен чистый MIDI-эффект, смени IS_SYNTH на FALSE
# и IS_MIDI_EFFECT на TRUE.
juce_add_plugin(DrUms
    COMPANY_NAME                desNotto
    PLUGIN_MANUFACTURER_CODE    Dsnt
    PLUGIN_CODE                 DrUm
    PRODUCT_NAME                "dr. Ums"
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    IS_MIDI_EFFECT              FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

# Картинка барабанной установки вшивается в плагин (см. Resources/kit.png)
juce_add_binary_data(DrUmsData SOURCES Resources/kit.png)
set_target_properties(DrUmsData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(DrUms PRIVATE
    Source/Model.cpp
    Source/DrumMaps.cpp
    Source/Generator.cpp
    Source/Corpus.cpp
    Source/MidiTools.cpp
    Source/GrooveLibrary.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DrUms PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_options(DrUms PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(DrUms
    PRIVATE
        DrUmsData
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
