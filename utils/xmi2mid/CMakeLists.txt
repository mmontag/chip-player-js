add_executable(xmi2mid xmi2mid.cpp)

install(TARGETS xmi2mid
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

