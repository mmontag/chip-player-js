add_executable(adlmidiplay
    adlmidiplay.cpp
    wave_writer.c
)

if(NOT ADLMIDI_DOS AND NOT MIDIPLAY_WAVE_ONLY)
    libADLMIDI_find_SDL2()
    target_link_libraries(adlmidiplay PRIVATE ADLMIDI_SDL2)
endif()

if(MIDIPLAY_WAVE_ONLY)
    target_compile_definitions(adlmidiplay PUBLIC OUTPUT_WAVE_ONLY)
    message("Demo tool will only output WAVE file, no playing support.")
endif()

if(ADLMIDI_DOS)
    target_compile_definitions(adlmidiplay PUBLIC HARDWARE_OPL3)
    message("Turn on hardware OPL3 support on demo tool")
endif()

target_link_libraries(adlmidiplay PRIVATE ADLMIDI)

if(WIN32)
    if(NOT MSVC)
        target_link_libraries(adlmidiplay PRIVATE pthread)
    endif()
elseif(NOT ADLMIDI_DOS)
    if(NOT OPENBSD_LOCALBASE AND NOT HAIKU)
        target_link_libraries(adlmidiplay PRIVATE dl)
    endif()
    target_link_libraries(adlmidiplay PRIVATE pthread m)
endif()

if(ADLMIDI_DOS)
    set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi)
endif()

if(TARGET ADLMIDI_shared)
    set_target_properties(adlmidiplay PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
