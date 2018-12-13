cmake_minimum_required (VERSION 3.2)
project(adlmidiplay CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(adlmidiplay
    adlmidiplay.cpp
    wave_writer.c
)

if(NOT MSDOS AND NOT DJGPP AND NOT MIDIPLAY_WAVE_ONLY)
    find_package(SDL2 REQUIRED)
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
    target_include_directories(adlmidiplay PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(adlmidiplay PRIVATE ${SDL2_LIBRARIES})
endif()

if(MIDIPLAY_WAVE_ONLY)
    target_compile_definitions(adlmidiplay PUBLIC OUTPUT_WAVE_ONLY)
    message("Demo tool will only output WAVE file, no playing support.")
endif()

if(MSDOS OR DJGPP)
    target_compile_definitions(adlmidiplay PUBLIC HARDWARE_OPL3)
    message("Turn on hardware OPL3 support on demo tool")
endif()

target_link_libraries(adlmidiplay PRIVATE ADLMIDI_IF)

if(WIN32)
    if(NOT MSVC)
        target_link_libraries(adlmidiplay PRIVATE pthread)
    endif()
elseif(NOT MSDOS AND NOT DJGPP)
    if(NOT OPENBSD_LOCALBASE)
        target_link_libraries(adlmidiplay PRIVATE dl)
    endif()
    target_link_libraries(adlmidiplay PRIVATE pthread m stdc++)
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

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
