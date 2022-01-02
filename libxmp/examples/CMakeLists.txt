
option(XMP_DEMO_WITH_SDL1      "Build XMP demo player using SDL-1.2" OFF)
if(XMP_DEMO_WITH_SDL1)
    add_executable(xmpsdl
        player-sdl.c
    )
    add_dependencies(xmpsdl XMP_IF)

    find_package(SDL REQUIRED)

    target_include_directories(xmpsdl PRIVATE ${SDL_INCLUDE_DIR})
    target_link_libraries(xmpsdl XMP_IF ${SDL_LIBRARY})
endif()

option(XMP_DEMO_WITH_SDL2      "Build XMP demo player using SDL-2.0" OFF)
if(XMP_DEMO_WITH_SDL2)
    add_executable(xmpsdl2
        player-sdl.c
    )
    add_dependencies(xmpsdl2 XMP_IF)

    find_package(SDL2 REQUIRED)
    if(TARGET SDL2::SDL2)
        set(SDL2_LIBRARIES SDL2::SDL2)
    endif()

    if(WIN32)
        target_compile_definitions(xmpsdl2 PRIVATE -DSDL_MAIN_HANDLED)
    endif()
    target_include_directories(xmpsdl2 PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(xmpsdl2 XMP_IF ${SDL2_LIBRARIES})
endif()
