find_package(SDL2 REQUIRED)
string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)

add_executable(adlmidi_sdl2_demo sdl2_sample.c)
target_include_directories(adlmidi_sdl2_demo PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(adlmidi_sdl2_demo PRIVATE ADLMIDI ${SDL2_LIBRARIES})

set_nopie(adlmidi_sdl2_demo)

if(NOT MSVC)
    target_link_libraries(adlmidi_sdl2_demo PRIVATE pthread)
endif()
if(NOT WIN32)
    target_link_libraries(adlmidi_sdl2_demo PRIVATE dl m)
endif()

if(TARGET ADLMIDI_shared)
    set_target_properties(adlmidi_sdl2_demo PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
endif()

install(TARGETS adlmidi_sdl2_demo
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

