cmake_minimum_required (VERSION 3.2)
project(adlmidi_vlc_plugin C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_library(SDL2_LIBRARY SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
message("Found ${SDL2_LIBRARY}")

add_executable(adlmidi_sdl2_demo sdl2_sample.c)

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

install(TARGETS adlmidi_sdl2_demo
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

