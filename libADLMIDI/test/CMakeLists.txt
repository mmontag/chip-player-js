
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(bankmap)
add_subdirectory(conversion)
add_subdirectory(wopl-file)

add_library(Catch-objects OBJECT "common/catch_main.cpp")
target_include_directories(Catch-objects PRIVATE "common")
