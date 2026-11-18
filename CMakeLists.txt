cmake_minimum_required(VERSION 3.22)

project(Tandem VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TD_BUILD_PLUGIN "Build the plugin (downloads JUCE)" ON)
option(TD_BUILD_TESTS "Build the engine and plugin tests" ON)

# The engine: header-only, no dependencies.
add_library(td_engine INTERFACE)
target_include_directories(td_engine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/engine)

if(TD_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests tests/engine_tests.cpp)
    target_link_libraries(engine_tests PRIVATE td_engine)
    if(NOT MSVC)
        target_compile_options(engine_tests PRIVATE -O2 -Wall -Wextra)
    endif()
    add_test(NAME engine_tests COMMAND engine_tests)
endif()

if(TD_BUILD_PLUGIN)
    include(FetchContent)
    if(DEFINED ENV{JUCE_DIR} AND EXISTS "$ENV{JUCE_DIR}/CMakeLists.txt")
        add_subdirectory($ENV{JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)
    else()
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 9.0.4
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    # An Audio Unit instrument for Logic. (Elsewhere a VST3 is built, only so the code can be checked.)
    if(APPLE)
        set(TD_FORMATS AU)
    else()
        set(TD_FORMATS VST3)
    endif()

    juce_add_plugin(Tandem
        PRODUCT_NAME "Tandem"
        PLUGIN_NAME "Tandem"
        COMPANY_NAME "Rumpus Room Records"
        BUNDLE_ID com.rumpusroomrecords.tandem
        PLUGIN_MANUFACTURER_CODE SamP
        PLUGIN_CODE Tndm
        FORMATS ${TD_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        VST3_CATEGORIES Instrument Synth)

    # The rendered panel and parts (art/), and the display's font: Barlow Semi
    # Condensed (SIL Open Font License, plugin/fonts).
    file(GLOB TD_ASSETS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/plugin/assets/*)
    juce_add_binary_data(TandemData SOURCES
        ${TD_ASSETS}
        plugin/fonts/BarlowSemiCondensed-SemiBold.ttf)
    set_target_properties(TandemData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    set(TD_PLUGIN_SOURCES plugin/PluginProcessor.cpp plugin/PluginEditor.cpp)
    set(TD_DEFINES JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_sources(Tandem PRIVATE ${TD_PLUGIN_SOURCES})
    target_include_directories(Tandem PRIVATE plugin)
    target_compile_definitions(Tandem PUBLIC ${TD_DEFINES})
    target_link_libraries(Tandem
        PRIVATE
            td_engine
            TandemData
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)

    if(TD_BUILD_TESTS)
        # End-to-end: the real processor, run the way a host runs it.
        juce_add_console_app(plugin_selftest PRODUCT_NAME "plugin_selftest")
        target_sources(plugin_selftest PRIVATE tests/plugin_selftest.cpp ${TD_PLUGIN_SOURCES})
        target_include_directories(plugin_selftest PRIVATE plugin tests)
        target_compile_definitions(plugin_selftest PRIVATE ${TD_DEFINES} JucePlugin_VersionString="${PROJECT_VERSION}")
        target_link_libraries(plugin_selftest PRIVATE td_engine TandemData juce::juce_audio_utils juce::juce_recommended_config_flags)
        add_test(NAME plugin_selftest COMMAND plugin_selftest)

        # Renders the window to PNGs, for checking it without a host.
        juce_add_console_app(ui_snapshot PRODUCT_NAME "ui_snapshot")
        target_sources(ui_snapshot PRIVATE tests/ui_snapshot.cpp ${TD_PLUGIN_SOURCES})
        target_include_directories(ui_snapshot PRIVATE plugin tests)
        target_compile_definitions(ui_snapshot PRIVATE ${TD_DEFINES} JucePlugin_VersionString="${PROJECT_VERSION}")
        target_link_libraries(ui_snapshot PRIVATE td_engine TandemData juce::juce_audio_utils juce::juce_recommended_config_flags)

        # Mac: the installed Audio Unit, hosted through Core Audio as Logic hosts it (run after installing).
        if(APPLE)
            juce_add_console_app(au_host PRODUCT_NAME "au_host")
            target_sources(au_host PRIVATE tests/au_host.cpp)
            target_compile_definitions(au_host PRIVATE JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
            target_link_libraries(au_host PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
        endif()
    endif()
endif()
