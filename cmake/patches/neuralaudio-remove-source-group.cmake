# Run by FetchContent (PATCH_COMMAND) inside the downloaded NeuralAudio source.
# NeuralAudio's CMakeLists files have malformed source_group() calls that newer CMake
# versions reject ("source_group given unknown argument"). They only sort files into
# folders in the Visual Studio file tree, so they are safe to drop. Safe to run twice.
foreach(f "NeuralAudio/CMakeLists.txt" "NeuralAudioCAPI/CMakeLists.txt")
    if(EXISTS "${f}")
        file(READ "${f}" content)
        string(REGEX REPLACE "\n[ \t]*source_group\\([^\n]*\\)" "\n# (source_group removed by Melon Plugins patch)" patched "${content}")
        if(NOT patched STREQUAL content)
            file(WRITE "${f}" "${patched}")
            message(STATUS "Melon: patched source_group() calls in ${f}")
        endif()
    endif()
endforeach()
