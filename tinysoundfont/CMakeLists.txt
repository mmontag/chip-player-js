cmake_minimum_required(VERSION 3.1)
project(tinyplayer)
add_library(${PROJECT_NAME} STATIC tinyplayer.c showcqtbar.c)
include_directories(~/src/emsdk/emscripten/1.38.11/system/include/emscripten)
