# This CMakeLists.txt is currently only used to load project files for CLion.
cmake_minimum_required(VERSION 3.10)

project(chip-core)

add_subdirectory(fluidlite)
add_subdirectory(game-music-emu)
add_subdirectory(lazyusf2)
add_subdirectory(libADLMIDI)
add_subdirectory(psflib)
add_subdirectory(tinysoundfont)
