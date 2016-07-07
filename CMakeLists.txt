cmake_minimum_required(VERSION 3.1)

include(FindPkgConfig)

project(fluidlite)

list(APPEND HEADERS
    src/fluid_chan.h
    src/fluid_chorus.h
    src/fluid_conv.h
    src/fluid_defsfont.h
    src/fluid_gen.h
    src/fluid_hash.h
    src/fluid_list.h
    src/fluid_mod.h
    src/fluid_phase.h
    src/fluid_ramsfont.h
    src/fluid_rev.h
    src/fluid_settings.h
    src/fluid_sfont.h
    src/fluid_synth.h
    src/fluid_sys.h
    src/fluid_tuning.h
    src/fluid_voice.h
    src/fluidsynth_priv.h
    src/fluid_config.h
    src/fluid_midi.h
)

list(APPEND SOURCES
    src/fluid_chan.c
    src/fluid_chorus.c
    src/fluid_conv.c
    src/fluid_defsfont.c
    src/fluid_dsp_float.c
    src/fluid_gen.c
    src/fluid_hash.c
    src/fluid_list.c
    src/fluid_mod.c
    src/fluid_ramsfont.c
    src/fluid_rev.c
    src/fluid_settings.c
    src/fluid_synth.c
    src/fluid_sys.c
    src/fluid_tuning.c
    src/fluid_voice.c
)

# Dependencies:
pkg_check_modules(LIBVORBIS vorbis>=1.3.5)
if (NOT LIBVORBIS_FOUND)
    list(APPEND SOURCES
        libvorbis-1.3.5/lib/vorbisenc.c
        libvorbis-1.3.5/lib/info.c
        libvorbis-1.3.5/lib/analysis.c
        libvorbis-1.3.5/lib/bitrate.c
        libvorbis-1.3.5/lib/block.c
        libvorbis-1.3.5/lib/codebook.c
        libvorbis-1.3.5/lib/envelope.c
        libvorbis-1.3.5/lib/floor0.c
        libvorbis-1.3.5/lib/floor1.c
        libvorbis-1.3.5/lib/lookup.c
        libvorbis-1.3.5/lib/lpc.c
        libvorbis-1.3.5/lib/lsp.c
        libvorbis-1.3.5/lib/mapping0.c
        libvorbis-1.3.5/lib/mdct.c
        libvorbis-1.3.5/lib/psy.c
        libvorbis-1.3.5/lib/registry.c
        libvorbis-1.3.5/lib/res0.c
        libvorbis-1.3.5/lib/sharedbook.c
        libvorbis-1.3.5/lib/smallft.c
        libvorbis-1.3.5/lib/vorbisfile.c
        libvorbis-1.3.5/lib/window.c
        libvorbis-1.3.5/lib/synthesis.c
    )
    list(APPEND LIBVORBIS_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/libvorbis-1.3.5/include
        ${CMAKE_SOURCE_DIR}/libvorbis-1.3.5/lib
    )
    message(WARNING "Using libvorbis shipped sources.")
endif()

pkg_check_modules(LIBOGG ogg>=1.3.2)
if (NOT LIBOGG_FOUND)
    list(APPEND SOURCES
        libogg-1.3.2/src/bitwise.c
        libogg-1.3.2/src/framing.c
    )
    set(LIBOGG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libogg-1.3.2/include)
    message(WARNING "Using libogg shipped sources.")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBOGG_INCLUDE_DIRS})
include_directories(${LIBVORBIS_INCLUDE_DIRS})

add_library(${PROJECT_NAME} STATIC ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)

