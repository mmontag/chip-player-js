add_executable(adldumpbank dumpbank.cpp)

set_nopie(adldumpbank)

install(TARGETS adldumpbank
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

