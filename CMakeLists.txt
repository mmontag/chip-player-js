cmake_minimum_required (VERSION 3.2)
project (libADLMIDI VERSION 1.4.0 LANGUAGES C CXX)

include(GNUInstallDirs)

#===========================================================================================
# Strip garbage
if(APPLE)
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -dead_strip")
elseif(NOT MSVC AND NOT MSDOS)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -s -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -s -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,-s")
    set(LINK_FLAGS_RELEASE  "${LINK_FLAGS_RELEASE} -Wl,--gc-sections -Wl,-s")
endif()

if(NOT MSVC AND NOT MSDOS)
# Global optimization flags
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
# Turn on all warnings
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
# Deny undefined symbols
    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
        set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined" )
    endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

string(TOLOWER ${CMAKE_BUILD_TYPE} CMAKE_BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE_LOWER EQUAL "release")
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
                $<$<COMPILE_LANGUAGE:C>:-Wall -pedantic -std=c90 -Wno-long-long>
                $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -std=gnu++98>
            )
        endif()
    endif()
endfunction()

function(set_visibility_hidden destTarget)
    if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$" AND NOT DJGPP)
        if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_GREATER 3.2)
            target_compile_options(${destTarget} PRIVATE
              $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden> -fvisibility=hidden)
        else()
            target_compile_options(${destTarget} PRIVATE -fvisibility=hidden)
        endif()
        target_compile_definitions(${destTarget} PRIVATE -DLIBADLMIDI_VISIBILITY)
    endif()
endfunction()
#===========================================================================================

option(libADLMIDI_STATIC   "Build static library of libADLMIDI" ON)
option(libADLMIDI_SHARED   "Build shared library of libADLMIDI" OFF)

option(WITH_CPP_EXTRAS      "Build with support for C++ extras (features are can be found in 'adlmidi.hpp' header)" OFF)
option(WITH_MIDI_SEQUENCER  "Build with embedded MIDI sequencer. Disable this if you want use library in real-time MIDI drivers or plugins.)" ON)
option(WITH_EMBEDDED_BANKS  "Use embedded banks" ON)
option(WITH_HQ_RESAMPLER    "Build with support for high quality resampling" OFF)
option(WITH_MUS_SUPPORT     "Build with support for DMX MUS files)" ON)
option(WITH_XMI_SUPPORT     "Build with support for AIL XMI files)" ON)
option(USE_DOSBOX_EMULATOR  "Use DosBox 0.74 OPL3 emulator (semi-accurate, suggested for slow or mobile platforms)" ON)
option(USE_NUKED_EMULATOR   "Use Nuked OPL3 emulator (most accurate, powerful)" ON)

option(WITH_GENADLDATA      "Build and run full rebuild of embedded banks cache" OFF)
option(WITH_GENADLDATA_COMMENTS "Enable comments in generated ADLDATA cache file" OFF)

option(WITH_MIDIPLAY        "Build also demo MIDI player" OFF)
option(MIDIPLAY_WAVE_ONLY   "Build Demo MIDI player without support of real time playing. It will output into WAV only." OFF)
option(WITH_ADLMIDI2        "Build also classic ADLMIDI player [EXPERIMENTAL]" OFF)
option(WITH_VLC_PLUGIN      "Build also a plugin for VLC Media Player" OFF)
option(VLC_PLUGIN_NOINSTALL "Don't install VLC plugin into VLC directory" OFF)
option(WITH_OLD_UTILS       "Build also old utilities" OFF)
option(WITH_XMI2MID         "Build a XMI to MIDI converter" OFF)
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

if(WITH_ADLMIDI2 OR (WITH_EMBEDDED_BANKS AND WITH_GENADLDATA))
    find_package(OpenMP)
endif()

if(WITH_HQ_RESAMPLER)
    find_library(ZITA_RESAMPLER_LIBRARY "zita-resampler" REQUIRED)
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
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGW" OR
                CMAKE_COMPILER_IS_MINGW)
            if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_GREATER 3.2)
                message("Turned on C++11 on GCC/CLang/Intel/MinGW")
                set_target_properties(gen_adldata PROPERTIES
                                      CXX_STANDARD 11
                                      CXX_STANDARD_REQUIRED YES)
                if(OPENMP_FOUND)
                    target_compile_options(gen_adldata PUBLIC "-fopenmp")
                    target_link_libraries(gen_adldata "-fopenmp")
                endif()
            endif()
        else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            message("Turned on C++11 on MSVC")
            target_compile_options(gen_adldata PUBLIC "/std:c++latest")
        endif()

        if(WITH_GENADLDATA_COMMENTS)
            target_compile_options(gen_adldata PUBLIC "-DADLDATA_WITH_COMMENTS")
        endif()

        set(ADLDATA_DATABASE
            "${libADLMIDI_SOURCE_DIR}/src/adldata.cpp"
        )
        add_custom_target(gen-adldata-run #OUTPUT ${ADLDATA_DATABASE}
            COMMAND gen_adldata "${ADLDATA_DATABASE}"
            WORKING_DIRECTORY ${libADLMIDI_SOURCE_DIR}
            DEPENDS gen_adldata "${libADLMIDI_SOURCE_DIR}/banks.ini"
            COMMENT "Running Embedded FM banks database generation"
            VERBATIM
        )
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

if(WITH_MIDI_SEQUENCER)
    list(APPEND libADLMIDI_SOURCES
        ${libADLMIDI_SOURCE_DIR}/src/adlmidi_sequencer.cpp
    )
    add_definitions(-DENABLE_END_SILENCE_SKIPPING)
endif()

if(NOT WITH_MUS_SUPPORT OR NOT WITH_MIDI_SEQUENCER)
    add_definitions(-DBWMIDI_DISABLE_MUS_SUPPORT)
endif()

if(NOT WITH_XMI_SUPPORT OR NOT WITH_MIDI_SEQUENCER)
    add_definitions(-DBWMIDI_DISABLE_XMI_SUPPORT)
endif()

if(NOT DJGPP AND NOT MSDOS)
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

# === Static library ====
if(libADLMIDI_STATIC OR WITH_VLC_PLUGIN)
    add_library(ADLMIDI_static STATIC ${libADLMIDI_SOURCES})
    set_target_properties(ADLMIDI_static PROPERTIES OUTPUT_NAME ADLMIDI)
    target_include_directories(ADLMIDI_static PUBLIC ${libADLMIDI_SOURCE_DIR}/include)
    set_legacy_standard(ADLMIDI_static)
    set_visibility_hidden(ADLMIDI_static)
    list(APPEND libADLMIDI_INSTALLS ADLMIDI_static)
    if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA)
        add_dependencies(ADLMIDI_static gen-adldata-run)
    endif()
    if(NOT libADLMIDI_STATIC)
        set_target_properties(ADLMIDI_static PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif()

# === Shared library ====
if(libADLMIDI_SHARED)
    add_library(ADLMIDI_shared SHARED ${libADLMIDI_SOURCES})
    set_target_properties(ADLMIDI_shared PROPERTIES OUTPUT_NAME ADLMIDI)
    target_include_directories(ADLMIDI_shared PUBLIC ${libADLMIDI_SOURCE_DIR}/include)
    set_legacy_standard(ADLMIDI_shared)
    set_visibility_hidden(ADLMIDI_shared)
    list(APPEND libADLMIDI_INSTALLS ADLMIDI_shared)
    if(WIN32)
        target_compile_definitions(ADLMIDI_shared PRIVATE "-DADLMIDI_BUILD_DLL")
        if(CMAKE_COMPILER_IS_GNUCXX)
            set_property(TARGET ADLMIDI_shared APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
            set_property(TARGET ADLMIDI_shared APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
        endif()
    endif()
    if(WITH_EMBEDDED_BANKS AND WITH_GENADLDATA)
        add_dependencies(ADLMIDI_shared gen-adldata-run)
    endif()
    set_target_properties(ADLMIDI_shared PROPERTIES SOVERSION "1")
endif()

if(NOT libADLMIDI_STATIC AND NOT libADLMIDI_SHARED)
    message(FATAL_ERROR "Both static and shared libADLMIDI builds are disabled!
You must enable at least one of them!")
endif()

add_library(ADLMIDI_IF INTERFACE)

if(libADLMIDI_STATIC OR WITH_VLC_PLUGIN)
    add_library(ADLMIDI_IF_STATIC INTERFACE)
    target_link_libraries(ADLMIDI_IF_STATIC INTERFACE ADLMIDI_static)
endif()

if(libADLMIDI_SHARED)
    target_link_libraries(ADLMIDI_IF INTERFACE ADLMIDI_shared)
else()
    target_link_libraries(ADLMIDI_IF INTERFACE ADLMIDI_static)
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
        target_compile_definitions(adlmidiplay PUBLIC "-DOUTPUT_WAVE_ONLY")
        message("Demo tool will only output WAVE file, no playing support.")
    endif()

    if(MSDOS OR DJGPP)
        target_compile_definitions(adlmidiplay PUBLIC "-DHARDWARE_OPL3")
        message("Turn on hardware OPL3 support on demo tool")
    endif()

    if(WIN32)
        if(MSVC)
            target_link_libraries(adlmidiplay ADLMIDI_IF ${SDL2_LIBRARY})
        else()
            target_link_libraries(adlmidiplay ADLMIDI_IF ${SDL2_LIBRARY} pthread)
        endif()
    else()
        if(MSDOS OR DJGPP)
            target_link_libraries(adlmidiplay ADLMIDI_IF)
        else()
            target_link_libraries(adlmidiplay ADLMIDI_IF ${SDL2_LIBRARY} pthread dl m stdc++)
        endif()
    endif()

    if(MSDOS OR DJGPP)
        set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi)
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidiplay ADLMIDI_shared)
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
        target_compile_definitions(adlmidi2 PUBLIC "-DSUPPORT_PUZZLE_GAME")
    endif()

    if(ADLMIDI2_VIDEO_OUT_SUPPORT)
        target_compile_definitions(adlmidi2 PUBLIC "-DSUPPORT_VIDEO_OUTPUT")
    endif()

    if(MSDOS OR DJGPP)
        target_compile_definitions(adlmidi2 PUBLIC "-DHARDWARE_OPL3")
        message("Turn on hardware OPL3 support on ADLMIDI2 tool")
    endif()

    if(WIN32)
        target_link_libraries(adlmidi2 ADLMIDI_IF winmm)
    elseif(DJGPP OR MSDOS)
        target_link_libraries(adlmidi2 ADLMIDI_IF)
    else()
        target_link_libraries(adlmidi2 ADLMIDI_IF ${SDL2_LIBRARY} pthread dl m stdc++)
    endif()

    if(NOT WITH_CPP_EXTRAS)
        message(FATAL_ERROR "ADLMIDI2 Requires C++ Extras. Please enable WITH_CPP_EXTRAS option!")
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR
                "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGW" OR
                CMAKE_COMPILER_IS_MINGW)
        if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} VERSION_GREATER 3.2)
            message("Turned on C++11 on GCC/CLang/Intel")
            set_target_properties(adlmidi2 PROPERTIES
                                      CXX_STANDARD 11
                                      CXX_STANDARD_REQUIRED YES)
        endif()
        if(OPENMP_FOUND)
            target_compile_options(adlmidi2 PUBLIC "-fopenmp")
            target_link_libraries(adlmidi2 "-fopenmp")
        endif()
    else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message("Turned on C++11 on MSVC")
        target_compile_options(adlmidi2 PUBLIC "/std:c++latest")
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidi2 ADLMIDI_shared)
        if(NOT MSVC)
            set_target_properties(adlmidi2 PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
        endif()
    else()
        if(NOT libADLMIDI_STATIC)
            message(FATAL_ERROR "libADLMIDI is required to be built!")
        endif()
        add_dependencies(adlmidi2 ADLMIDI_static)
    endif()

    list(APPEND libADLMIDI_INSTALLS adlmidi2)
endif()

if(WITH_XMI2MID)
  add_executable(xmi2mid
    ${libADLMIDI_SOURCE_DIR}/utils/xmi2mid/xmi2mid.cpp)
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
            target_link_libraries(adlmidi_sdl2_demo ADLMIDI_IF ${SDL2_LIBRARY})
        else()
            target_link_libraries(adlmidi_sdl2_demo ADLMIDI_IF ${SDL2_LIBRARY} pthread)
        endif()
    else()
        target_link_libraries(adlmidi_sdl2_demo ADLMIDI_IF ${SDL2_LIBRARY} pthread dl m stdc++)
    endif()

    if(MSDOS OR DJGPP)
        set_target_properties(adlmidi_sdl2_demo PROPERTIES OUTPUT_NAME adlmidi)
    endif()

    if(libADLMIDI_SHARED)
        add_dependencies(adlmidi_sdl2_demo ADLMIDI_shared)
        set_target_properties(adlmidi_sdl2_demo PROPERTIES COMPILE_FLAGS "-Wl,-rpath='$$ORIGIN/../lib'")
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

    add_library(adlmidi_vlc_plugin MODULE ${adlmidi_vlc_src})
    set_target_properties(adlmidi_vlc_plugin PROPERTIES OUTPUT_NAME adlmidi_plugin)
    target_compile_options(adlmidi_vlc_plugin PUBLIC
                "-DVLC_MODULE_COPYRIGHT=\"Copyright \(c\) Vitaly Novichkov\""
                "-DVLC_MODULE_LICENSE=\"GPLv3\""
                "-DMODULE_STRING=\"adlmidi\""
                ${VLCPLUGIN_DEFINITIONS})
    target_include_directories(adlmidi_vlc_plugin PRIVATE
            ${LIBVLC_INCLUDE_DIR}/vlc/plugins
            ${LIBVLC_INCLUDE_DIR}/vlc)

    set(ADLMIDIVLC_LIBS ADLMIDI_IF_STATIC ${LIBVLCCORE_LIBRARY} m)

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_COMPILER_IS_GNUCXX)
        set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -static-libgcc -static-libstdc++")
        set_property(TARGET adlmidi_vlc_plugin APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bstatic,--whole-archive -lpthread -Wl,-Bdynamic,--no-whole-archive")
    elseif(NOT WIN32)
        list(APPEND ADLMIDIVLC_LIBS stdc++)
    endif()

    target_link_libraries(adlmidi_vlc_plugin ${ADLMIDIVLC_LIBS})
    # target_link_libraries(adlmidi_vlc_plugin INTERFACE "-export-symbol-regex ^vlc_entry")

    if(NOT VLC_PLUGIN_NOINSTALL)
        install(TARGETS adlmidi_vlc_plugin DESTINATION "${VLCPLUGIN_CODEC_INSTALL_PATH}")
    else()
        install(TARGETS adlmidi_vlc_plugin DESTINATION "${CMAKE_INSTALL_LIBDIR}/vlc-codec/")
    endif()

    add_dependencies(adlmidi_vlc_plugin ADLMIDI_static)
endif()

if(WITH_HQ_RESAMPLER)
    if(libADLMIDI_SHARED)
        target_compile_definitions(ADLMIDI_shared PRIVATE -DADLMIDI_ENABLE_HQ_RESAMPLER)
        target_link_libraries(ADLMIDI_shared PUBLIC "${ZITA_RESAMPLER_LIBRARY}")
    endif()
    if(libADLMIDI_STATIC OR WITH_VLC_PLUGIN)
        target_compile_definitions(ADLMIDI_static PRIVATE -DADLMIDI_ENABLE_HQ_RESAMPLER)
        target_link_libraries(ADLMIDI_static PUBLIC "${ZITA_RESAMPLER_LIBRARY}")
    endif()
endif()

install(TARGETS ${libADLMIDI_INSTALLS}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(FILES
        include/adlmidi.h
        include/adlmidi.hpp
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(NOT ANDROID)
    configure_file(libADLMIDI.pc.in libADLMIDI.pc @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libADLMIDI.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

option(WITH_UNIT_TESTS "Enable unit testing" OFF)
if(WITH_UNIT_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# === Version check ====
set(VERSION_CHECK_SOURCE "
    #include \"adlmidi.h\"
    #if !(ADLMIDI_VERSION_MAJOR == ${PROJECT_VERSION_MAJOR} && ADLMIDI_VERSION_MINOR == ${PROJECT_VERSION_MINOR} && ADLMIDI_VERSION_PATCHLEVEL == ${PROJECT_VERSION_PATCH})
    #error Project and source code version do not match!
    #endif")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/version_check.c" "${VERSION_CHECK_SOURCE}")
add_library(ADLMIDI_version_check OBJECT "${CMAKE_CURRENT_BINARY_DIR}/version_check.c")
target_include_directories(ADLMIDI_version_check PRIVATE "include")

message("==== libADLMIDI options ====")
message("libADLMIDI_STATIC        = ${libADLMIDI_STATIC}")
message("libADLMIDI_SHARED        = ${libADLMIDI_SHARED}")
message("WITH_UNIT_TESTS          = ${WITH_UNIT_TESTS}")

message("WITH_CPP_EXTRAS          = ${WITH_CPP_EXTRAS}")
message("WITH_MIDI_SEQUENCER      = ${WITH_MIDI_SEQUENCER}")
message("WITH_EMBEDDED_BANKS      = ${WITH_EMBEDDED_BANKS}")
message("WITH_HQ_RESAMPLER        = ${WITH_HQ_RESAMPLER}")
message("WITH_MUS_SUPPORT         = ${WITH_MUS_SUPPORT}")
message("WITH_XMI_SUPPORT         = ${WITH_XMI_SUPPORT}")
message("USE_DOSBOX_EMULATOR      = ${USE_DOSBOX_EMULATOR}")
message("USE_NUKED_EMULATOR       = ${USE_NUKED_EMULATOR}")

message("===== Utils and extras =====")
message("WITH_GENADLDATA          = ${WITH_GENADLDATA}")
message("WITH_GENADLDATA_COMMENTS = ${WITH_GENADLDATA_COMMENTS}")

message("WITH_MIDIPLAY            = ${WITH_MIDIPLAY}")
message("MIDIPLAY_WAVE_ONLY       = ${MIDIPLAY_WAVE_ONLY}")
message("WITH_ADLMIDI2            = ${WITH_ADLMIDI2}")
message("WITH_VLC_PLUGIN          = ${WITH_VLC_PLUGIN}")
message("WITH_OLD_UTILS           = ${WITH_OLD_UTILS}")
message("WITH_XMI2MID             = ${WITH_XMI2MID}")
message("EXAMPLE_SDL2_AUDIO       = ${EXAMPLE_SDL2_AUDIO}")
