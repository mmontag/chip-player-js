cmake_minimum_required (VERSION 3.2)
project(adlmidi2 CXX)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenMP)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

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
    midiplay.cc
    input.cc
)

if(ADLMIDI2_HAS_PUZZLE_GAME)
    list(APPEND adlmidi2_src puzzlegame.cc)
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
    set(ADLMIDI2_LIBS ADLMIDI_IF ${SDL2_LIBRARY} pthread dl m stdc++)
    if(OPENBSD_LOCALBASE)
        list(REMOVE_ITEM ADLMIDI2_LIBS dl)
    endif()
    target_link_libraries(adlmidi2 ${ADLMIDI2_LIBS})
endif()

if(NOT WITH_CPP_EXTRAS)
    message(FATAL_ERROR "ADLMIDI2 Requires C++ Extras. Please enable WITH_CPP_EXTRAS option!")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel" OR
            "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MinGW" OR
            CMAKE_COMPILER_IS_MINGW)
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

install(TARGETS adlmidi2
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

