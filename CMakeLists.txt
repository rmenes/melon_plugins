cmake_minimum_required(VERSION 3.22)

project(MelonPlugins LANGUAGES C CXX) # version: MELON_VERSION in cmake/MelonCommon.cmake

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
add_subdirectory(plugins/Comp)
add_subdirectory(plugins/ProEQ)
add_subdirectory(plugins/Shifter)
add_subdirectory(plugins/Tuner)
add_subdirectory(plugins/SimpleGate)
add_subdirectory(plugins/MultiDelay)
add_subdirectory(plugins/MultiMod)
add_subdirectory(plugins/Nam)
add_subdirectory(plugins/Cab)
