
if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include/libxmp-lite
    ${CMAKE_SOURCE_DIR}/src)

add_executable(libxmp-test test.c ../src/md5.c)
set_property(TARGET libxmp-test PROPERTY C_STANDARD 90)
if(BUILD_SHARED) # Make sure unit test will find its DLLs while running
    set_property(TARGET libxmp-test PROPERTY RUNTIME_OUTPUT_DIRECTORY ${libxmp_BINARY_DIR})
endif()

target_link_libraries(libxmp-test XMP_IF)

add_test(NAME libxmp-test COMMAND libxmp-test WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
