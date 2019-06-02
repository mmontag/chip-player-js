cmake_minimum_required(VERSION 3.4.1)

set(src_DIR ../src)
set(include_DIR ../include)

set(libADLMIDI_sources
        ${src_DIR}/adldata.cpp
        ${src_DIR}/adlmidi_load.cpp
        ${src_DIR}/adlmidi_midiplay.cpp
        ${src_DIR}/adlmidi_mus2mid.c
        ${src_DIR}/adlmidi_opl3.cpp
        ${src_DIR}/adlmidi_private.cpp
        ${src_DIR}/adlmidi_xmi2mid.c
        ${src_DIR}/adlmidi.cpp
        ${src_DIR}/wopl/wopl_file.c
        ${src_DIR}/chips/dosbox_opl3.cpp
        ${src_DIR}/chips/nuked_opl3_v174.cpp
        ${src_DIR}/chips/nuked_opl3.cpp
        ${src_DIR}/chips/opl_chip_base.cpp
        ${src_DIR}/chips/dosbox/dbopl.cpp
        ${src_DIR}/chips/nuked/nukedopl3_174.c
        ${src_DIR}/chips/nuked/nukedopl3.c
)

add_library(libADLMIDI SHARED ${libADLMIDI_sources})

# Specify directories which the compiler should look for headers
target_include_directories(libADLMIDI PRIVATE
                            ${include_DIR}
)

target_include_directories(libADLMIDI INTERFACE
                            ${include_DIR}
)