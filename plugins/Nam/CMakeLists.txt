cmake_minimum_required(VERSION 3.22)

project(MelonNam LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonCommon.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonNeuralAudio.cmake) # NeuralAudio model engine

juce_add_plugin(Nam
    VERSION                   ${MELON_VERSION}
    COMPANY_NAME              "${MELON_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE  ${MELON_MANUFACTURER_CODE}
    PRODUCT_NAME              "Melon NAM"
    ICON_BIG                  "${MELON_BRANDING_DIR}/png/melon-icon-512.png"
    ICON_SMALL                "${MELON_BRANDING_DIR}/png/melon-icon-128.png"
    PLUGIN_CODE               Nam1
    FORMATS                   ${MELON_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Distortion
    VST3_AUTO_MANIFEST        ${MELON_VST3_MANIFEST}
    LV2URI                    "${MELON_LV2_URI_BASE}/melon-nam")

target_sources(Nam PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NeuralModelSlot.cpp)

target_include_directories(Nam PRIVATE Source ${MELON_NEURALAUDIO_SRC})

melon_configure_plugin(Nam) # no network: no TONE3000, no libcurl

target_link_libraries(Nam PRIVATE NeuralAudio)

# NeuralAudio is a C++20 library
target_compile_features(Nam PRIVATE cxx_std_20)

# DSP test (independent of JUCE and of the neural engine)
add_executable(Nam_test_dsp tests/test_dsp.cpp)
target_include_directories(Nam_test_dsp PRIVATE Source ${MELON_SHARED_DIR})
