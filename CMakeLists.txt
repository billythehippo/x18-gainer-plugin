cmake_minimum_required(VERSION 3.15)

# 1. Инициализация проекта
project(x18-gainer VERSION 1.0.0)

# 2. Подключаем JUCE
find_package(JUCE CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)

# 3. Объявляем плагин
juce_add_plugin(x18-gainer
    COMPANY_NAME "HipposMusicBox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    # Включаем нужные форматы
    FORMATS VST3 AU LV2
    PRODUCT_NAME "X18 Preamp Control Plugin"
    LV2URI "http://github.com/billythehippo/x18-gainer-plugin"
    )

if(UNIX AND NOT APPLE)
    #find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

    # Привязываем GTK3 к вашему таргету (замените x18 на ваше имя)
    target_link_libraries(x18-gainer PRIVATE PkgConfig::GTK3)

    # На некоторых версиях Debian нужно явно пробросить инклюды
    target_include_directories(x18-gainer PRIVATE ${GTK3_INCLUDE_DIRS})
endif()

juce_generate_juce_header(x18-gainer)
# 4. Добавляем ваши исходники
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

# 5. Подключаем модули JUCE (те самые, что были в Projucer)
target_link_libraries(x18-gainer PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_audio_utils
    juce::juce_events)

target_compile_features(x18-gainer PRIVATE cxx_std_17)

target_compile_definitions(x18-gainer PRIVATE
    JUCE_WEB_BROWSER=0
    # Здесь же можно отключить и другие ненужные модули
    JUCE_USE_CURL=0
    JucePlugin_LV2URI="http://github.com/billythehippo/x18-gainer-plugin"
)
