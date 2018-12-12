cmake_minimum_required (VERSION 3.2)
project(adlmidiplay CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(NOT MSDOS AND NOT DJGPP AND NOT MIDIPLAY_WAVE_ONLY)
    find_library(SDL2_LIBRARY SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIR})
    message("Found ${SDL2_LIBRARY}")
else()
    unset(SDL2_LIBRARY)
endif()

add_executable(adlmidiplay
    adlmidiplay.cpp
    wave_writer.c
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
        set(ADLMIDIPLAY_LIBS ADLMIDI_IF ${SDL2_LIBRARY} pthread dl m stdc++)
        if(OPENBSD_LOCALBASE)
            list(REMOVE_ITEM ADLMIDIPLAY_LIBS dl)
        endif()
        target_link_libraries(adlmidiplay ${ADLMIDIPLAY_LIBS})
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

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

