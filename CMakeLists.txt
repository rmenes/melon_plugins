cmake_minimum_required(VERSION 3.22)

project(MelonPlugins VERSION 1.0.0 LANGUAGES C CXX)

include(cmake/MelonCommon.cmake)

# Add every new plugin in the series here
add_subdirectory(plugins/GraphicEQ)
add_subdirectory(plugins/Chorus)
add_subdirectory(plugins/Flanger)
add_subdirectory(plugins/GuitarSynth)
add_subdirectory(plugins/Dirt)
add_subdirectory(plugins/Muffs)
add_subdirectory(plugins/GatesOfDoom)
add_subdirectory(plugins/Reverberare)
add_subdirectory(plugins/Delay2290)
add_subdirectory(plugins/Rack)
