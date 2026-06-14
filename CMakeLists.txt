# This CMakeLists.txt is currently only used to load project files for CLion.
cmake_minimum_required(VERSION 3.10)
project(chip-core)
set(CMAKE_CXX_STANDARD 14)

#add_subdirectory(fluidlite)
#add_subdirectory(game-music-emu)
#add_subdirectory(lazyusf2)
#add_subdirectory(libADLMIDI)
#add_subdirectory(psflib)
#add_subdirectory(mdxmini)
#add_subdirectory(libxmp)
#add_subdirectory(libvgm)

add_library(chip-core
    src/bindings/libsidplayfp-wrapper.cpp
    src/bindings/libvgm-wrapper.cpp
)

target_include_directories(chip-core
    PRIVATE
    ../libsidplayfp/src
    ../libvgm
    "$ENV{EMSDK}/upstream/emscripten/system/include"
)
