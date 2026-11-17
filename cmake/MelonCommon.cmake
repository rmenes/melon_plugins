# MelonCommon.cmake - settings shared by every Melon plugin.
# Included by the root CMakeLists.txt and by each plugin's CMakeLists.txt
# (so each plugin can be built on its own or as part of the series).
include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Series identity (the same for every plugin) ------------------------------
set(MELON_COMPANY_NAME      "Melon")
set(MELON_MANUFACTURER_CODE "Meln")
set(MELON_LV2_URI_BASE      "https://github.com/melon-audio")

# --- Build options -------------------------------------------------------------
set(MELON_FORMATS VST3 LV2 Standalone CACHE STRING "Formats to build (VST3, LV2, Standalone, AU)")
option(MELON_VST3_MANIFEST "Generate the VST3 moduleinfo.json (turn off when cross-compiling)" ON)
set(JUCE_DIR "" CACHE PATH "Local copy of JUCE (optional). Empty = download from GitHub")

# --- JUCE (downloaded once for every plugin) -----------------------------------
if(NOT TARGET juce::juce_core)
    if(JUCE_DIR)
        add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.4
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

# --- Branding (logo shared by every plugin) -----------------------------------
set(MELON_BRANDING_DIR ${CMAKE_CURRENT_LIST_DIR}/../branding)

# --- Shared code (DSP blocks, processor/editor base classes, test helpers) ----
set(MELON_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/../shared)

if(NOT TARGET MelonBranding)
    # Generates MelonBranding.h with MelonBranding::melonlogoondarkheader_png, etc.
    juce_add_binary_data(MelonBranding
        HEADER_NAME MelonBranding.h
        NAMESPACE   MelonBranding
        SOURCES     ${MELON_BRANDING_DIR}/png/melon-logo-ondark-header.png)
    set_target_properties(MelonBranding PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

# Applies the series' standard definitions and flags to a plugin
function(melon_configure_plugin target)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_include_directories(${target} PRIVATE ${MELON_SHARED_DIR})

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            MelonBranding
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wno-float-equal)
    endif()
endfunction()
