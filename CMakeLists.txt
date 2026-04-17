cmake_minimum_required(VERSION 3.15)

project(x18-gainer VERSION 1.0.0)

find_package(JUCE CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)

juce_add_plugin(x18-gainer
    COMPANY_NAME "HipposMusicBox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    # Включаем нужные форматы
    FORMATS VST3 AU LV2
    PRODUCT_NAME "x18-gainer"
    LV2URI "http://github.com/billythehippo/x18-gainer-plugin"
    )

if(UNIX AND NOT APPLE)
    #find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    target_link_libraries(x18-gainer PRIVATE PkgConfig::GTK3)
    target_include_directories(x18-gainer PRIVATE ${GTK3_INCLUDE_DIRS})
endif()

juce_generate_juce_header(x18-gainer)

target_sources(x18-gainer PRIVATE
    Source/dial.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/tinyosc.c
    Source/tinyosc.h
    Source/x18.h
)

target_link_libraries(x18-gainer PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_audio_utils
    juce::juce_events)

target_compile_features(x18-gainer PRIVATE cxx_std_17)

target_compile_definitions(x18-gainer PUBLIC 
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_compile_definitions(x18-gainer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_LV2URI="http://github.com/billythehippo/x18-gainer-plugin"
)
