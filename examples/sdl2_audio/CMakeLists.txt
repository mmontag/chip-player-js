cmake_minimum_required (VERSION 3.2)
project(adlmidi_vlc_plugin C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(SDL2 REQUIRED)
string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)

add_executable(adlmidi_sdl2_demo sdl2_sample.c)
target_include_directories(adlmidi_sdl2_demo PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(adlmidi_sdl2_demo PRIVATE ADLMIDI_IF ${SDL2_LIBRARIES})

if(NOT MSVC)
    target_link_libraries(adlmidi_sdl2_demo PRIVATE pthread)
endif()
if(NOT WIN32)
    target_link_libraries(adlmidi_sdl2_demo PRIVATE dl m stdc++)
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
