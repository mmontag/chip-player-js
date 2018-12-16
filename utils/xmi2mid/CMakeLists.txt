add_executable(xmi2mid xmi2mid.cpp)
# TODO: Use own library
target_include_directories(xmi2mid PRIVATE ${PROJECT_SOURCE_DIR}/src)

install(TARGETS xmi2mid
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

