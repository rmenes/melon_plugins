cmake_minimum_required(VERSION 3.22)

project(MelonCab LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonCommon.cmake)

juce_add_plugin(Cab
    VERSION                   ${MELON_VERSION}
    COMPANY_NAME              "${MELON_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE  ${MELON_MANUFACTURER_CODE}
    PRODUCT_NAME              "Melon Cab"
    ICON_BIG                  "${MELON_BRANDING_DIR}/png/melon-icon-512.png"
    ICON_SMALL                "${MELON_BRANDING_DIR}/png/melon-icon-128.png"
    PLUGIN_CODE               Cab1
    FORMATS                   ${MELON_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Filter
    VST3_AUTO_MANIFEST        ${MELON_VST3_MANIFEST}
    LV2URI                    "${MELON_LV2_URI_BASE}/melon-cab")

target_sources(Cab PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Cab PRIVATE Source)

melon_configure_plugin(Cab)

target_link_libraries(Cab PRIVATE juce::juce_dsp)

# DSP test (independent of JUCE)
add_executable(Cab_test_dsp tests/test_dsp.cpp)
target_include_directories(Cab_test_dsp PRIVATE Source ${MELON_SHARED_DIR})

# Library scan test (shared by Melon NAM and Melon Cab; independent of JUCE)
add_executable(Melon_test_library_scan ${MELON_SHARED_DIR}/tests/test_library_scan.cpp)
target_include_directories(Melon_test_library_scan PRIVATE ${MELON_SHARED_DIR})
