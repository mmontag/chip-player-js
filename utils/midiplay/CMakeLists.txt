add_executable(adlmidiplay
    adlmidiplay.cpp
    wave_writer.c
)

if(NOT ADLMIDI_DOS AND NOT MIDIPLAY_WAVE_ONLY)
    find_package(SDL2 REQUIRED)
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
    target_include_directories(adlmidiplay PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(adlmidiplay PRIVATE ${SDL2_LIBRARIES})
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
    if(NOT OPENBSD_LOCALBASE)
        target_link_libraries(adlmidiplay PRIVATE dl)
    endif()
    target_link_libraries(adlmidiplay PRIVATE pthread m stdc++)
endif()

if(ADLMIDI_DOS)
    set_target_properties(adlmidiplay PROPERTIES OUTPUT_NAME adlmidi)
endif()

install(TARGETS adlmidiplay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
