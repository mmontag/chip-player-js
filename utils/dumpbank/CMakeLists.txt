set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(adldumpbank dumpbank.cpp)

install(TARGETS adldumpbank
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

