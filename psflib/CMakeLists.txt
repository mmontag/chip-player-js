# This CMakeLists.txt is currently only used to load project files for CLion.
cmake_minimum_required(VERSION 3.10)
project(psflib C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(psflib
        psf2fs.c
        psf2fs.h
        psflib.c
        psflib.h)
