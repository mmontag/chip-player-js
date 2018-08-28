#
#  Project file for the Qt Creator IDE
#

TEMPLATE = lib
CONFIG  -= qt
CONFIG  += staticlib

TARGET = ADLMIDI
INSTALLINCLUDES = $$PWD/include/*
INSTALLINCLUDESTO = ADLMIDI
include($$PWD/../audio_codec_common.pri)

DEFINES += ADLMIDI_DISABLE_CPP_EXTRAS
DEFINES += ENABLE_END_SILENCE_SKIPPING

macx: QMAKE_CXXFLAGS_WARN_ON += -Wno-absolute-value

INCLUDEPATH += $$PWD $$PWD/include

HEADERS += \
    include/adlmidi.h \
    src/adlbank.h \
    src/adldata.hh \
    src/adlmidi_private.hpp \
    src/wopl/wopl_file.h \
    src/chips/dosbox/dbopl.h \
    src/chips/dosbox_opl3.h \
    src/chips/nuked/nukedopl3_174.h \
    src/chips/nuked/nukedopl3.h \
    src/chips/nuked_opl3.h \
    src/chips/nuked_opl3_v174.h \
    src/chips/opl_chip_base.h \
    src/chips/opl_chip_base.tcc \
    src/cvt_mus2mid.hpp \
    src/cvt_xmi2mid.hpp \
    src/midi_sequencer.h \
    src/midi_sequencer.hpp \
    src/midi_sequencer_impl.hpp \
    src/fraction.hpp

SOURCES += \
    src/adldata.cpp \
    \
    src/adlmidi.cpp \
    src/adlmidi_load.cpp \
    src/adlmidi_midiplay.cpp \
    src/adlmidi_opl3.cpp \
    src/adlmidi_private.cpp \
    src/adlmidi_sequencer.cpp \
    src/wopl/wopl_file.c \
    src/chips/dosbox/dbopl.cpp \
    src/chips/dosbox_opl3.cpp \
    src/chips/nuked/nukedopl3_174.c \
    src/chips/nuked/nukedopl3.c \
    src/chips/nuked_opl3.cpp \
    src/chips/nuked_opl3_v174.cpp

