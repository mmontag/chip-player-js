add_executable(adldumpbank dumpbank.cpp)

install(TARGETS adldumpbank
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

