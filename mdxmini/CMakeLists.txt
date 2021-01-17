cmake_minimum_required(VERSION 3.10)
project(mdxmini C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(src)
include_directories(~/src/emsdk/emscripten/1.38.11/system/include/emscripten)

add_executable(mdxmini
        src/class.h
        src/mdx.h
        src/mdx2151.c
        src/mdx2151.h
        src/mdxfile.c
        src/mdxmini.c
        src/mdxmini.h
        src/mdxmml_ym2151.c
        src/nlg.c
        src/nlg.h
        src/pcm8.c
        src/pcm8.h
        src/pdxfile.c
        src/version.h
        src/ym2151.c
        src/ym2151.h)
