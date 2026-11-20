cmake_minimum_required(VERSION 3.22)

project(MelonRack LANGUAGES C CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/MelonCommon.cmake)

# --- NeuralAudio (NAM / AIDA-X / RTNeural model engine, MIT licence) ----------------
# Downloaded once, like JUCE. Set NEURALAUDIO_DIR to use a local clone instead
# (git clone --recurse-submodules https://github.com/mikeoliphant/NeuralAudio).
set(NEURALAUDIO_DIR "" CACHE PATH "Local copy of NeuralAudio (optional). Empty = download from GitHub")
# Pinned to a known-good commit; change it to update the engine (the API changes between versions)
set(NEURALAUDIO_TAG "29b90ac311abf0ec507abb3f16a90ffda151d592" CACHE STRING "NeuralAudio commit, tag or branch to download")

if(NOT TARGET NeuralAudio)
    # Only the library, not NeuralAudio's test utilities. NAM Core stays on: NeuralAudio
    # falls back to it for NAM architectures its own engine doesn't cover (e.g. custom A2 models).
    set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
    set(BUILD_NAMCORE ON CACHE BOOL "" FORCE)

    if(NEURALAUDIO_DIR)
        add_subdirectory(${NEURALAUDIO_DIR} ${CMAKE_BINARY_DIR}/NeuralAudio EXCLUDE_FROM_ALL)
        set(MELON_NEURALAUDIO_SRC ${NEURALAUDIO_DIR})
    else()
        include(FetchContent)
        FetchContent_Declare(NeuralAudio
            GIT_REPOSITORY https://github.com/mikeoliphant/NeuralAudio.git
            GIT_TAG        ${NEURALAUDIO_TAG}
            GIT_SUBMODULES_RECURSE TRUE
            # Newer CMake (e.g. GitHub's Windows image from 2026-11) rejects some source_group() lines in NeuralAudio
            PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/../../cmake/patches/neuralaudio-remove-source-group.cmake)
        FetchContent_MakeAvailable(NeuralAudio)
        set(MELON_NEURALAUDIO_SRC ${neuralaudio_SOURCE_DIR})
    endif()
    set_target_properties(NeuralAudio PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

# --- the plugin ---------------------------------------------------------------------------
juce_add_plugin(Rack
    VERSION                   ${MELON_VERSION}
    COMPANY_NAME              "${MELON_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE  ${MELON_MANUFACTURER_CODE}
    PRODUCT_NAME              "Melon Rack"
    ICON_BIG                  "${MELON_BRANDING_DIR}/png/melon-icon-512.png"
    ICON_SMALL                "${MELON_BRANDING_DIR}/png/melon-icon-128.png"
    PLUGIN_CODE               Rck1
    FORMATS                   ${MELON_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    NEEDS_CURL                TRUE      # TONE3000 downloads (HTTPS) on Linux
    VST3_CATEGORIES           Fx Distortion
    VST3_AUTO_MANIFEST        ${MELON_VST3_MANIFEST}
    LV2URI                    "${MELON_LV2_URI_BASE}/melon-rack")

target_sources(Rack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/NeuralModelSlot.cpp
    Source/Tone3000.cpp
    Source/ToneStrip.cpp)

target_include_directories(Rack PRIVATE Source ${MELON_NEURALAUDIO_SRC})

melon_configure_plugin(Rack NETWORK)

# TONE3000 integration: the publishable key (cmake/MelonCommon.cmake) and, if present,
# the official logo (branding/tone3000/tone3000-logo.png, from the TONE3000 logo pack).
target_compile_definitions(Rack PRIVATE "MELON_TONE3000_CLIENT_ID=\"${MELON_TONE3000_CLIENT_ID}\"")
set(MELON_T3K_LOGO ${MELON_BRANDING_DIR}/tone3000/tone3000-logo.png)
if(EXISTS ${MELON_T3K_LOGO})
    if(NOT TARGET Tone3000Logo)
        juce_add_binary_data(Tone3000Logo
            HEADER_NAME Tone3000Logo.h
            NAMESPACE   Tone3000Logo
            SOURCES     ${MELON_T3K_LOGO})
        set_target_properties(Tone3000Logo PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    endif()
    target_link_libraries(Rack PRIVATE Tone3000Logo)
    target_compile_definitions(Rack PRIVATE MELON_HAS_T3K_LOGO=1)
endif()

target_link_libraries(Rack PRIVATE NeuralAudio juce::juce_dsp)

# NeuralAudio is a C++20 library
target_compile_features(Rack PRIVATE cxx_std_20)

# DSP test (independent of JUCE and of the neural engine)
add_executable(Rack_test_dsp tests/test_dsp.cpp)
target_include_directories(Rack_test_dsp PRIVATE Source ${MELON_SHARED_DIR})
