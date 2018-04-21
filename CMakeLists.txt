cmake_minimum_required (VERSION 3.2)
project (libADLMIDI)

#===========================================================================================
# Strip garbage
if(APPLE)
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
ELSEIF(NOT MSVC AND NOT MSDOS)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -s -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -s -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
ENDIF()

# Global optimization flags
IF(NOT MSVC AND NOT MSDOS)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
ENDIF()

if (CMAKE_BUILD_TYPE EQUAL "RELEASE")
    add_definitions(-DNDEBUG)
ENDIF()

# Disable bogus MSVC warnings
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
ENDIF()

# -fPIC thing
IF(NOT WIN32 AND NOT DJGPP AND NOT MSDOS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
ENDIF()

if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_LESS 3.2)
    message("==Legacy CMake detected! Turning C++11 for everything==")
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
        # Turn on C++11 for everything
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
endif()

function(set_legacy_standard destTarget)
    if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_GREATER 3.2)
        if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
            # Turn on warnings and legacy C/C++ standards to support more compilers
            target_compile_options(${destTarget} PRIVATE
                $<$<COMPILE_LANGUAGE:C>:-Wall -pedantic -std=c90>
                $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -std=gnu++98>
            )
        endif()
    endif()
endfunction()
#===========================================================================================

option(WITH_MIDIPLAY        "Build also demo MIDI player" OFF)
option(MIDIPLAY_WAVE_ONLY   "Build Demo MIDI player without support of real time playing. It will output into WAV only." OFF)
option(WITH_ADLMIDI2        "Build also classic ADLMIDI player [EXPERIMENTAL]" OFF)
option(WITH_VLC_PLUGIN      "Build also a plugin for VLC Media Player" OFF)
option(WITH_OLD_UTILS       "Build also old utilities" OFF)
option(WITH_EMBEDDED_BANKS  "Use embedded banks" ON)
option(WITH_GENADLDATA      "Build and run full rebuild of embedded banks cache" OFF)
option(WITH_GENADLDATA_COMMENTS "Enable comments in generated ADLDATA cache file" OFF)
option(USE_DOSBOX_EMULATOR  "Use DosBox 0.74 OPL3 emulator (semi-accurate, suggested for slow or mobile platforms)" ON)
option(USE_NUKED_EMULATOR   "Use Nuked OPL3 emulator (most accurate, powerful)" ON)
option(WITH_CPP_EXTRAS      "Build with support for C++ extras (features are can be found in 'adlmidi.hpp' header)" OFF)
option(WITH_MIDI_SEQUENCER  "Build with embedded MIDI sequencer. Disable this if you want use library in real-time MIDI drivers or plugins.)" ON)
option(WITH_MUS_SUPPORT     "Build with support for DMX MUS files)" ON)
option(WITH_XMI_SUPPORT     "Build with support for AIL XMI files)" ON)

option(libADLMIDI_STATIC   "Build static library of libADLMIDI" ON)
option(libADLMIDI_SHARED   "Build shared library of libADLMIDI" OFF)

option(EXAMPLE_SDL2_AUDIO   "Build also a simple SDL2 demo MIDI player" OFF)

if(CMAKE_VERSION VERSION_EQUAL "3.1" OR CMAKE_VERSION VERSION_GREATER "3.1")
    set(CMAKE_CXX_STANDARD 11)
endif()

set(libADLMIDI_INSTALLS)

include_directories(${libADLMIDI_SOURCE_DIR}/include)
include_directories(${libADLMIDI_SOURCE_DIR}/src/)
link_directories(${libADLMIDI_BINARY_DIR}/)

if(WITH_OLD_UTILS)
    # Old utility to dump AdLib bank files
    add_executable(adldumpbank
        ${libADLMIDI_SOURCE_DIR}/utils/dumpbank/dumpbank.cpp
    )
    list(APPEND libADLMIDI_INSTALLS adldumpbank)

    # Old utility to dump AIL bank files
    add_executable(adldumpmiles
        ${libADLMIDI_SOURCE_DIR}/utils/dumpmiles/dumpmiles.cpp
    )
    list(APPEND libADLMIDI_INSTALLS adldumpmiles)
endif()

if(WITH_EMBEDDED_BANKS)
    if(WITH_GENADLDATA)
        set(GEN_ADLDATA_SRC)
        list(APPEND GEN_ADLDATA_SRC
            ${libADLMIDI_SOURCE_DIR}/utils/gen_adldata/gen_adldata.cc
            ${libADLMIDI_SOURCE_DIR}/utils/gen_adldata/progs_cache.cpp
            ${libADLMIDI_SOURCE_DIR}/utils/gen_adldata/measurer.cpp
            ${libADLMIDI_SOURCE_DIR}/utils/gen_adldata/ini/ini_processing.cpp
        )

        list(APPEND GEN_ADLDATA_SRC
            ${libADLMIDI_SOURCE_DIR}/src/chips/opl_chip_base.cpp
        )

        if(USE_DOSBOX_EMULATOR)
            set(HAS_EMULATOR TRUE)
            list(APPEND GEN_ADLDATA_SRC
                ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox_opl3.cpp
                ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox/dbopl.cpp
            )
        else()
            add_definitions(-DADLMIDI_DISABLE_DOSBOX_EMULATOR)
        endif()

        if(USE_NUKED_EMULATOR)
            set(HAS_EMULATOR TRUE)
            list(APPEND GEN_ADLDATA_SRC
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3.cpp       # v 1.8
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3.c
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3_v174.cpp  # v 1.7.4
                ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3_174.c
            )
        else()
            add_definitions(-DADLMIDI_DISABLE_NUKED_EMULATOR)
        endif()

        add_executable(gen_adldata
            ${GEN_ADLDATA_SRC}
        )
        if(NOT MSVC)
            target_link_libraries(gen_adldata pthread m stdc++)
        endif()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
                CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
            if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.2)
                message("Turned on C++11 on GCC/CLang/Intel")
                target_compile_options(gen_adldata PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++11>)
            endif()
        else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            message("Turned on C++11 on MSVC")
            target_compile_options(gen_adldata PUBLIC "/std:c++latest")
        endif()

        if(WITH_GENADLDATA_COMMENTS)
            target_compile_options(gen_adldata PUBLIC "-DADLDATA_WITH_COMMENTS")
        endif()

        add_custom_target(
            gen-adldata-run
            COMMAND gen_adldata ${libADLMIDI_SOURCE_DIR}/src/adldata.cpp
            WORKING_DIRECTORY ${libADLMIDI_SOURCE_DIR}
        )
        add_dependencies(gen-adldata-run gen_adldata)
    endif()
endif()


set(libADLMIDI_SOURCES)

list(APPEND libADLMIDI_SOURCES
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_load.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_midiplay.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_opl3.cpp
    ${libADLMIDI_SOURCE_DIR}/src/adlmidi_private.cpp
    ${libADLMIDI_SOURCE_DIR}/src/wopl/wopl_file.c
)

if(WITH_MUS_SUPPORT AND WITH_MIDI_SEQUENCER)
    list(APPEND libADLMIDI_SOURCES
        ${libADLMIDI_SOURCE_DIR}/src/adlmidi_mus2mid.c
    )
else()
    add_definitions(-DADLMIDI_DISABLE_MUS_SUPPORT)
endif()

if(WITH_XMI_SUPPORT AND WITH_MIDI_SEQUENCER)
    list(APPEND libADLMIDI_SOURCES
        ${libADLMIDI_SOURCE_DIR}/src/adlmidi_xmi2mid.c
    )
else()
    add_definitions(-DADLMIDI_DISABLE_XMI_SUPPORT)
endif()

if(NOT DJGPP AND NOT MSDOS)
    list(APPEND libADLMIDI_SOURCES
        ${libADLMIDI_SOURCE_DIR}/src/chips/opl_chip_base.cpp
    )

    if(USE_DOSBOX_EMULATOR)
        set(HAS_EMULATOR TRUE)
        list(APPEND libADLMIDI_SOURCES
            ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox_opl3.cpp
            ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox/dbopl.cpp
        )
    else()
        add_definitions(-DADLMIDI_DISABLE_DOSBOX_EMULATOR)
    endif()

    if(USE_NUKED_EMULATOR)
        set(HAS_EMULATOR TRUE)
        list(APPEND libADLMIDI_SOURCES
            ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3.cpp       # v 1.8
            ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3.c
            ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3_v174.cpp  # v 1.7.4
            ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3_174.c
        )
    else()
        add_definitions(-DADLMIDI_DISABLE_NUKED_EMULATOR)
    endif()

    if(NOT HAS_EMULATOR)
        message(FATAL_ERROR "No emulators enabled! You must enable at least one emulator!")
    endif()
endif()

if(WITH_EMBEDDED_BANKS)
    list(APPEND libADLMIDI_SOURCES
        ${libADLMIDI_SOURCE_DIR}/src/adldata.cpp
    )
else()
    add_definitions(-DDISABLE_EMBEDDED_BANKS)
endif()

if(NOT WITH_MIDI_SEQUENCER)
    add_definitions(-DADLMIDI_DISABLE_MIDI_SEQUENCER)
endif()

if(NOT WITH_CPP_EXTRAS)
    add_definitions(-DADLMIDI_DISABLE_CPP_EXTRAS)
endif()

if(libADLMIDI_STATIC)
    add_library(ADLMIDI_static STATIC ${libADLMIDI_SOURCES})
    set_target_properties(ADLMIDI_static PROPERTIES OUTPUT_NAME ADLMIDI)
    target_include_directories(ADLMIDI_static PUBLIC ${libADLMIDI_SOURCE_DIR}/include)
    set_legacy_standard(ADLMIDI_static)
    list(APPEND libADLMIDI_INSTALLS ADLMIDI_static)
    if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA)
        add_dependencies(ADLMIDI_static gen-adldata-run)
    endif()
endif()

if(libADLMIDI_SHARED)
    add_library(ADLMIDI_shared SHARED ${libADLMIDI_SOURCES})
    set_target_properties(ADLMIDI_shared PROPERTIES OUTPUT_NAME ADLMIDI)
    target_include_directories(ADLMIDI_shared PUBLIC ${libADLMIDI_SOURCE_DIR}/include)
    set_legacy_standard(ADLMIDI_shared)
    list(APPEND libADLMIDI_INSTALLS ADLMIDI_shared)
    if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA)
        add_dependencies(ADLMIDI_shared gen-adldata-run)
    endif()
endif()

if(NOT libADLMIDI_STATIC AND NOT libADLMIDI_SHARED)
    message(FATAL_ERROR "Both static and shared libADLMIDI builds are disabled!
You must enable at least one of them!")
endif()

add_library(ADLMIDI INTERFACE)
if(libADLMIDI_SHARED)
  target_link_libraries(ADLMIDI INTERFACE ADLMIDI_shared)
else()
  target_link_libraries(ADLMIDI INTERFACE ADLMIDI_static)
endif()

if(WITH_MIDIPLAY)
    if(NOT MSDOS AND NOT DJGPP AND NOT MIDIPLAY_WAVE_ONLY)
        find_library(SDL2_LIBRARY SDL2 REQUIRED)
        include_directories(${SDL2_INCLUDE_DIR})
        message("Found ${SDL2_LIBRARY}")
    else()
        unset(SDL2_LIBRARY)
    endif()

    add_executable(adlmidiplay
        ${libADLMIDI_SOURCE_DIR}/utils/midiplay/adlmidiplay.cpp
        ${libADLMIDI_SOURCE_DIR}/utils/midiplay/wave_writer.c
    )

    if(MIDIPLAY_WAVE_ONLY)
        target_compile_options(adlmidiplay PUBLIC "-DOUTPUT_WAVE_ONLY")
        message("Demo tool will only output WAVE file, no playing support.")
    endif()

    if(MSDOS OR DJGPP)
        target_compile_options(adlmidiplay PUBLIC "-DHARDWARE_OPL3")
        message("Turn on hardware OPL3 support on demo tool")
    endif()

    if(WIN32)
        if(MSVC)
            target_link_libraries(adlmidiplay ADLMIDI ${SDL2_LIBRARY})
        else()
            target_link_libraries(adlmidiplay ADLMIDI ${SDL2_LIBRARY} pthread)
        endif()
    else()
        if(MSDOS OR DJGPP)
            target_link_libraries(adlmidiplay ADLMIDI)
        else()
            target_link_libraries(adlmidiplay ADLMIDI ${SDL2_LIBRARY} pthread dl m stdc++)
        endif()
    endif()

    if(MSDOS OR DJGPP)
        set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi)
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidiplay ADLMIDI_shared)
        # ========= WIP =========
        # set_target_properties(adlmidiplay PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
    else()
        if(NOT libADLMIDI_STATIC)
            message(FATAL_ERROR "libADLMIDI is required to be built!")
        endif()
        add_dependencies(adlmidiplay ADLMIDI_static)
    endif()

    list(APPEND libADLMIDI_INSTALLS adlmidiplay)
endif()

if(WITH_ADLMIDI2)
    if(NOT WIN32 AND NOT DJGPP AND NOT MSDOS)
        find_library(SDL2_LIBRARY SDL2 REQUIRED)
        include_directories(${SDL2_INCLUDE_DIR})
        message("Found ${SDL2_LIBRARY}")
    endif()

    if(NOT WIN32 AND NOT DJGPP AND NOT MSDOS)
        set(ADLMIDI2_HAS_PUZZLE_GAME "TRUE")
        set(ADLMIDI2_VIDEO_OUT_SUPPORT "TRUE")
    endif()

    set(adlmidi2_src)
    list(APPEND adlmidi2_src
        ${libADLMIDI_SOURCE_DIR}/utils/adlmidi-2/midiplay.cc
        ${libADLMIDI_SOURCE_DIR}/utils/adlmidi-2/input.cc)

    if(ADLMIDI2_HAS_PUZZLE_GAME)
        list(APPEND adlmidi2_src ${libADLMIDI_SOURCE_DIR}/utils/adlmidi-2/puzzlegame.cc)
    endif()

    add_executable(adlmidi2 ${adlmidi2_src})

    if(ADLMIDI2_HAS_PUZZLE_GAME)
        target_compile_options(adlmidi2 PUBLIC "-DSUPPORT_PUZZLE_GAME")
    endif()

    if(ADLMIDI2_VIDEO_OUT_SUPPORT)
        target_compile_options(adlmidi2 PUBLIC "-DSUPPORT_VIDEO_OUTPUT")
    endif()

    if(WIN32)
        target_link_libraries(adlmidi2 ADLMIDI winmm)
    elseif(DJGPP OR MSDOS)
        target_link_libraries(adlmidi2 ADLMIDI)
    else()
        target_link_libraries(adlmidi2 ADLMIDI ${SDL2_LIBRARY} pthread dl m stdc++)
    endif()

    if(NOT WITH_CPP_EXTRAS)
        message(FATAL_ERROR "ADLMIDI2 Requires C++ Extras. Please enable WITH_CPP_EXTRAS option!")
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
        if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.2)
            message("Turned on C++11 on GCC/CLang/Intel")
            target_compile_options(adlmidi2 PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++11>)
        endif()
    else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message("Turned on C++11 on MSVC")
        target_compile_options(adlmidi2 PUBLIC "/std:c++latest")
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidi2 ADLMIDI_shared)
        # ========= WIP =========
        # set_target_properties(adlmidiplay PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
    else()
        if(NOT libADLMIDI_STATIC)
            message(FATAL_ERROR "libADLMIDI is required to be built!")
        endif()
        add_dependencies(adlmidi2 ADLMIDI_static)
    endif()

    list(APPEND libADLMIDI_INSTALLS adlmidi2)
endif()

if(EXAMPLE_SDL2_AUDIO AND NOT MSDOS AND NOT DJGPP)
    find_library(SDL2_LIBRARY SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIR})
    message("Found ${SDL2_LIBRARY}")

    add_executable(adlmidi_sdl2_demo
        ${libADLMIDI_SOURCE_DIR}/examples/sd2_audio/sdl2_sample.c
    )

    if(WIN32)
        if(MSVC)
            target_link_libraries(adlmidi_sdl2_demo ADLMIDI ${SDL2_LIBRARY})
        else()
            target_link_libraries(adlmidi_sdl2_demo ADLMIDI ${SDL2_LIBRARY} pthread)
        endif()
    else()
        target_link_libraries(adlmidi_sdl2_demo ADLMIDI ${SDL2_LIBRARY} pthread dl m stdc++)
    endif()

    if(MSDOS OR DJGPP)
        set_target_properties(adlmidi_sdl2_demo PROPERTIES OUTPUT_NAME adlmidi)
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidi_sdl2_demo ADLMIDI_shared)
        # ========= WIP =========
        # set_target_properties(adlmidi_sdl2_demo PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
    else()
        if(NOT libADLMIDI_STATIC)
            message(FATAL_ERROR "libADLMIDI is required to be built!")
        endif()
        add_dependencies(adlmidi_sdl2_demo ADLMIDI_static)
    endif()

    list(APPEND libADLMIDI_INSTALLS adlmidi_sdl2_demo)
endif()

if(WITH_VLC_PLUGIN)
    include(cmake/FindLIBVLC.cmake)
    message("Found ${LIBVLCCORE_LIBRARY} of version ${LIBVLC_VERSION} with includes ${LIBVLC_INCLUDE_DIR}")

    set(adlmidi_vlc_src)
    list(APPEND adlmidi_vlc_src
        ${libADLMIDI_SOURCE_DIR}/utils/vlc_codec/libadlmidi.c)

    add_library(adlmidi_plugin MODULE ${adlmidi_vlc_src})
    target_compile_options(adlmidi_plugin PUBLIC
                "-DVLC_MODULE_COPYRIGHT=\"Copyright \(c\) Vitaly Novichkov\""
                "-DVLC_MODULE_LICENSE=\"GPLv3\""
                "-DMODULE_STRING=\"adlmidi\""
                ${VLCPLUGIN_DEFINITIONS})
    target_include_directories(adlmidi_plugin PRIVATE
            ${LIBVLC_INCLUDE_DIR}/vlc/plugins
            ${LIBVLC_INCLUDE_DIR}/vlc)
    target_link_libraries(adlmidi_plugin ADLMIDI vlccore m stdc++)
    # target_link_libraries(adlmidi_plugin INTERFACE "-export-symbol-regex ^vlc_entry")

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidi_plugin ADLMIDI_shared)
        # ========= WIP =========
        # set_target_properties(adlmidiplay PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
    else()
        if(NOT libADLMIDI_STATIC)
            message(FATAL_ERROR "libADLMIDI is required to be built!")
        endif()
        add_dependencies(adlmidi_plugin ADLMIDI_static)
    endif()

endif()

install(TARGETS ${libADLMIDI_INSTALLS}
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        INCLUDES DESTINATION "include")

install(FILES
        include/adlmidi.h
        include/adlmidi.hpp
        DESTINATION include/)

message("==== libADLMIDI options ====")
message("WITH_MIDIPLAY            = ${WITH_MIDIPLAY}")
message("WITH_VLC_PLUGIN          = ${WITH_VLC_PLUGIN}")
message("MIDIPLAY_WAVE_ONLY       = ${MIDIPLAY_WAVE_ONLY}")
message("WITH_ADLMIDI2            = ${WITH_ADLMIDI2}")
message("WITH_OLD_UTILS           = ${WITH_OLD_UTILS}")
message("WITH_EMBEDDED_BANKS      = ${WITH_EMBEDDED_BANKS}")
message("WITH_GENADLDATA          = ${WITH_GENADLDATA}")
message("WITH_GENADLDATA_COMMENTS = ${WITH_GENADLDATA_COMMENTS}")
message("USE_DOSBOX_EMULATOR      = ${USE_DOSBOX_EMULATOR}")
message("USE_NUKED_EMULATOR       = ${USE_NUKED_EMULATOR}")
message("WITH_MIDI_SEQUENCER      = ${WITH_MIDI_SEQUENCER}")
message("WITH_CPP_EXTRAS          = ${WITH_CPP_EXTRAS}")
message("WITH_MUS_SUPPORT         = ${WITH_MUS_SUPPORT}")
message("WITH_XMI_SUPPORT         = ${WITH_XMI_SUPPORT}")
message("libADLMIDI_STATIC        = ${libADLMIDI_STATIC}")
message("libADLMIDI_SHARED        = ${libADLMIDI_SHARED}")
message("EXAMPLE_SDL2_AUDIO       = ${EXAMPLE_SDL2_AUDIO}")

