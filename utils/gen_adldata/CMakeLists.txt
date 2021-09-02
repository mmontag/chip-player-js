cmake_minimum_required (VERSION 3.2)
project(gen_adldata CXX)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenMP QUIET)
if(OPENMP_FOUND)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -openmp:experimental")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -openmp:experimental")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(GEN_ADLDATA_SRC)
list(APPEND GEN_ADLDATA_SRC
    gen_adldata.cc
    progs_cache.cpp
    measurer.cpp
    ini/ini_processing.cpp
)

option(WITH_GENADLDATA_DEEPDEBUG "Enable deep debug mode of gen_adldata with dumping every voice into WAV file" OFF)
if(WITH_GENADLDATA_DEEPDEBUG)
    add_definitions(-DGEN_ADLDATA_DEEP_DEBUG)
    list(APPEND GEN_ADLDATA_SRC
        ../midiplay/wave_writer.c
    )
    file(GLOB WAV_FILES ${libADLMIDI_SOURCE_DIR}/fm_banks/_deep_debug/*.wav)
    if(WAV_FILES)
        file(REMOVE ${WAV_FILES})
    endif()
    add_custom_target(gen_adldata_deepdebug_prepare ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${libADLMIDI_SOURCE_DIR}/fm_banks/_deep_debug
        COMMAND ${CMAKE_COMMAND} -E remove ${libADLMIDI_SOURCE_DIR}/fm_banks/adldata-cache-x.dat
    )
endif()

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
if(MSVC)
    message("Turned on C++11 on MSVC")
    target_compile_options(gen_adldata PUBLIC "/std:c++latest")
else()
    target_link_libraries(gen_adldata PRIVATE pthread m)
endif()

set_nopie(gen_adldata)

if(WITH_GENADLDATA_DEEPDEBUG)
    add_dependencies(gen_adldata gen_adldata_deepdebug_prepare)
endif()

option(WITH_GENADLDATA_PROGRESS "Enable progress printing in gen_adldata" OFF)
if(WITH_GENADLDATA_PROGRESS OR WITH_GENADLDATA_DEEPDEBUG)
    target_compile_options(gen_adldata PUBLIC "-DADL_GENDATA_PRINT_PROGRESS")
endif()

option(WITH_GENADLDATA_MINIFY "Minify the printing result at gen_adldata" ON)
if(WITH_GENADLDATA_MINIFY)
    target_compile_options(gen_adldata PUBLIC "-DADL_GENDATA_MINIFY")
endif()

if(WITH_GENADLDATA_COMMENTS)
    target_compile_options(gen_adldata PUBLIC "-DADLDATA_WITH_COMMENTS")
endif()

set(ADLDATA_DATABASE
    "${libADLMIDI_SOURCE_DIR}/src/inst_db.cpp"
)
add_custom_target(gen-adldata-run #OUTPUT ${ADLDATA_DATABASE}
    COMMAND gen_adldata "${ADLDATA_DATABASE}"
    WORKING_DIRECTORY ${libADLMIDI_SOURCE_DIR}
    DEPENDS gen_adldata "${libADLMIDI_SOURCE_DIR}/banks.ini"
    COMMENT "Running Embedded FM banks database generation"
    VERBATIM
)
