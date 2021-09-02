add_executable(adldumpmiles dumpmiles.cpp)

set_nopie(adldumpmiles)

install(TARGETS adldumpmiles
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

