if(POLICY CMP0111)
    cmake_policy(SET CMP0111 NEW)
endif()

find_package(SDL2 REQUIRED)
add_library(ADLMIDIEXAMPLE_SDL2 INTERFACE)
if(TARGET SDL2::SDL2)
    if(MINGW)
        target_link_libraries(ADLMIDIEXAMPLE_SDL2 INTERFACE mingw32 SDL2::SDL2main SDL2::SDL2)
    else()
        target_link_libraries(ADLMIDIEXAMPLE_SDL2 INTERFACE SDL2::SDL2)
    endif()
    target_include_directories(ADLMIDIEXAMPLE_SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
else()
    string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
    target_include_directories(ADLMIDIEXAMPLE_SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ADLMIDIEXAMPLE_SDL2 INTERFACE ${SDL2_LIBRARIES})
endif()

add_executable(adlmidi_sdl2_demo sdl2_sample.c)
target_link_libraries(adlmidi_sdl2_demo PRIVATE ADLMIDI ADLMIDIEXAMPLE_SDL2)

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
