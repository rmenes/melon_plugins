cmake_minimum_required(VERSION 3.22)

project(MelonProEQ LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonCommon.cmake)

juce_add_plugin(ProEQ
    VERSION                   ${MELON_VERSION}
    COMPANY_NAME              "${MELON_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE  ${MELON_MANUFACTURER_CODE}
    PRODUCT_NAME              "Melon Pro EQ"
    ICON_BIG                  "${MELON_BRANDING_DIR}/png/melon-icon-512.png"
    ICON_SMALL                "${MELON_BRANDING_DIR}/png/melon-icon-128.png"
    PLUGIN_CODE               Peq1
    FORMATS                   ${MELON_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx EQ
    VST3_AUTO_MANIFEST        ${MELON_VST3_MANIFEST}
    LV2URI                    "${MELON_LV2_URI_BASE}/melon-proeq")

target_sources(ProEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(ProEQ PRIVATE Source)

melon_configure_plugin(ProEQ)

# The Linear Phase kernels are designed on a background thread
find_package(Threads REQUIRED)
target_link_libraries(ProEQ PRIVATE Threads::Threads)

# DSP test (independent of JUCE)
add_executable(ProEQ_test_dsp tests/test_dsp.cpp)
target_include_directories(ProEQ_test_dsp PRIVATE Source ${MELON_SHARED_DIR})
target_link_libraries(ProEQ_test_dsp PRIVATE Threads::Threads)
