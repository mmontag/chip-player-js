cmake_minimum_required (VERSION 3.2)
project(adldumpmiles CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(adldumpmiles dumpmiles.cpp)

install(TARGETS adldumpmiles
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

