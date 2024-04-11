if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()

if(POLICY CMP0115)
    cmake_policy(SET CMP0115 NEW)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src)

set(DEVTEST_SRC
    util.c
    main.c
    simple_module.c
    compare_mixer_data.c
    ../src/md5.c
    ../src/far_extras.c
    ../src/filetype.c
    ../src/hio.c
    ../src/lfo.c
    ../src/load_helpers.c
    ../src/period.c
    ../src/memio.c
    ../src/dataio.c
    ../src/scan.c
    ../src/loaders/itsex.c
    ../src/loaders/sample.c
    ../src/loaders/common.c
    ../src/depackers/xfnmatch.c
    ../src/win32.c
)

set(DEV_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/tests-dev-generated")
file(MAKE_DIRECTORY "${DEV_GENERATED}")
include_directories("${DEV_GENERATED}")


file(STRINGS "all_tests.txt" ALL_DEV_TESTS)
set(ALL_TESTS_C_CONTENT)

foreach(DEVTEST ${ALL_DEV_TESTS})
    list(APPEND DEVTEST_SRC ${DEVTEST}.c)
    set(ALL_TESTS_C_CONTENT "${ALL_TESTS_C_CONTENT}\ndeclare_test(${DEVTEST});")
endforeach()

file(WRITE "${DEV_GENERATED}/all_tests.c" "${ALL_TESTS_C_CONTENT}")

add_executable(libxmp-tests-dev ${DEVTEST_SRC})
set_property(TARGET libxmp-tests-dev PROPERTY C_STANDARD 90)

if(MSVC)
    target_compile_definitions(libxmp-tests-dev PRIVATE ${LIBXMP_MSVC_DEFINES})
endif()

if(BUILD_SHARED) # Make sure test.exe will find its dll
    set_property(TARGET libxmp-tests-dev PROPERTY RUNTIME_OUTPUT_DIRECTORY ${libxmp_BINARY_DIR})
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable")
endif()

option(LIBXMP_TEST_DEV_NO_FORK_TEST   "Disable the forked test at development unit tests" OFF)
if(LIBXMP_TEST_DEV_NO_FORK_TEST)
    target_compile_definitions(libxmp-tests-dev PRIVATE -DNO_FORK_TEST)
endif()

target_link_libraries(libxmp-tests-dev XMP_IF)
add_test(NAME libxmp-tests-dev COMMAND libxmp-tests-dev WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
