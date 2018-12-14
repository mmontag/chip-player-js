add_executable(adldumpmiles dumpmiles.cpp)

install(TARGETS adldumpmiles
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

