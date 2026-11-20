# MelonCommon.cmake - settings shared by every Melon plugin.
# Included by the root CMakeLists.txt and by each plugin's CMakeLists.txt
# (so each plugin can be built on its own or as part of the series).
include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Series identity (the same for every plugin) ------------------------------
# Version of every plugin in the series (shown under each plugin's title and
# reported to the host). Bump it here before a release, e.g. 1.2.0 -> 1.3.0.
set(MELON_VERSION           "1.2.0")

# TONE3000 publishable key (client_id) used by Melon Rack to sign in and download tones.
# Create it at https://www.tone3000.com/settings (API keys). It is meant to be public
# (it ships inside the plugin), so it can be written here. Never put the SECRET key here.
# One key for the app: every user still signs in with their own TONE3000 account.
# Empty = Melon Rack only opens the suggested tones in the browser.
# Can also be passed on the command line: -DMELON_TONE3000_CLIENT_ID=...
if(NOT DEFINED MELON_TONE3000_CLIENT_ID)
    set(MELON_TONE3000_CLIENT_ID "t3k_pub_UBVTD9VHp8JRasys1BgLrcgAtE_rkkKU")
endif()
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

# Applies the series' standard definitions and flags to a plugin.
# melon_configure_plugin(<target> NETWORK) for plugins that use HTTPS: on Linux this needs
# libcurl (also add NEEDS_CURL TRUE to juce_add_plugin). Windows and macOS use the OS.
function(melon_configure_plugin target)
    cmake_parse_arguments(MELON "NETWORK" "" "" ${ARGN})
    set(melon_use_curl 0)
    if(MELON_NETWORK AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(melon_use_curl 1)
    endif()

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=${melon_use_curl}
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
