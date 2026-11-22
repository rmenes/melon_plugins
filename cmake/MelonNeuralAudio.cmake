# MelonNeuralAudio.cmake - the NeuralAudio engine (NAM / AIDA-X / RTNeural models, MIT licence),
# for plugins that load neural models on their own (Melon NAM). Downloaded once, like JUCE.
# Melon Rack downloads the same engine in its own CMakeLists.txt; whichever comes first creates
# the NeuralAudio target and the other reuses it.
#
# After include(): target NeuralAudio exists and MELON_NEURALAUDIO_SRC is the folder to add to the
# include path (#include <NeuralAudio/NeuralModel.h>).
include_guard(DIRECTORY)

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
    else()
        include(FetchContent)
        FetchContent_Declare(NeuralAudio
            GIT_REPOSITORY https://github.com/mikeoliphant/NeuralAudio.git
            GIT_TAG        ${NEURALAUDIO_TAG}
            GIT_SUBMODULES_RECURSE TRUE
            # Newer CMake (e.g. GitHub's Windows image from 2026-11) rejects some source_group() lines in NeuralAudio
            PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/patches/neuralaudio-remove-source-group.cmake)
        FetchContent_MakeAvailable(NeuralAudio)
    endif()
    set_target_properties(NeuralAudio PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
endif()

# Where the headers are (works whether this file or Melon Rack created the target)
if(NEURALAUDIO_DIR)
    set(MELON_NEURALAUDIO_SRC ${NEURALAUDIO_DIR})
else()
    include(FetchContent)
    FetchContent_GetProperties(NeuralAudio SOURCE_DIR MELON_NEURALAUDIO_SRC)
endif()
