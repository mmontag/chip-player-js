cmake_minimum_required (VERSION 3.2)
project(gen_adldata CXX)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenMP)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(GEN_ADLDATA_SRC)
list(APPEND GEN_ADLDATA_SRC
    gen_adldata.cc
    progs_cache.cpp
    measurer.cpp
    ini/ini_processing.cpp
)

if(USE_DOSBOX_EMULATOR)
    set(HAS_EMULATOR TRUE)
    list(APPEND GEN_ADLDATA_SRC
        ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox_opl3.cpp
        ${libADLMIDI_SOURCE_DIR}/src/chips/dosbox/dbopl.cpp
    )
else()
    add_definitions(-DADLMIDI_DISABLE_DOSBOX_EMULATOR)
endif()

if(USE_NUKED_EMULATOR)
    set(HAS_EMULATOR TRUE)
    list(APPEND GEN_ADLDATA_SRC
        ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3.cpp       # v 1.8
        ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3.c
        ${libADLMIDI_SOURCE_DIR}/src/chips/nuked_opl3_v174.cpp  # v 1.7.4
        ${libADLMIDI_SOURCE_DIR}/src/chips/nuked/nukedopl3_174.c
    )
else()
    add_definitions(-DADLMIDI_DISABLE_NUKED_EMULATOR)
endif()

add_executable(gen_adldata
    ${GEN_ADLDATA_SRC}
)
if(NOT MSVC)
    target_link_libraries(gen_adldata PRIVATE pthread m stdc++)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel|MinGW" OR MINGW)
    if(OPENMP_FOUND)
        target_compile_options(gen_adldata PUBLIC "-fopenmp")
        target_link_libraries(gen_adldata PRIVATE "-fopenmp")
    endif()
else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message("Turned on C++11 on MSVC")
    target_compile_options(gen_adldata PUBLIC "/std:c++latest")
endif()

if(WITH_GENADLDATA_COMMENTS)
    target_compile_options(gen_adldata PUBLIC "-DADLDATA_WITH_COMMENTS")
endif()

set(ADLDATA_DATABASE
    "${libADLMIDI_SOURCE_DIR}/src/adldata.cpp"
)
add_custom_target(gen-adldata-run #OUTPUT ${ADLDATA_DATABASE}
    COMMAND gen_adldata "${ADLDATA_DATABASE}"
    WORKING_DIRECTORY ${libADLMIDI_SOURCE_DIR}
    DEPENDS gen_adldata "${libADLMIDI_SOURCE_DIR}/banks.ini"
    COMMENT "Running Embedded FM banks database generation"
    VERBATIM
)
