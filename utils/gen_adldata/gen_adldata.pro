TEMPLATE=app
CONFIG-=qt
CONFIG+=console
TARGET=gen_adldata
DESTDIR=$$PWD/../../bin
CONFIG += c++11

include($$PWD/ini/IniProcessor.pri)

#DEFINES += ADLMIDI_USE_DOSBOX_OPL

QMAKE_CXXFLAGS_RELEASE += -O3 -finline-functions
LIBS += -lpthread

HEADERS += \
    midi_inst_list.h \
    ../nukedopl3.h \
    ../dbopl.h \
    progs_cache.h \
    file_formats/load_bnk.h \
    file_formats/load_bnk2.h \
    file_formats/load_op2.h \
    file_formats/load_ail.h \
    file_formats/load_ibk.h \
    file_formats/load_jv.h \
    file_formats/load_tmb.h \
    file_formats/load_bisqwit.h \
    file_formats/load_wopl.h \
    measurer.h \
    file_formats/common.h \
    file_formats/load_ea.h

SOURCES += \
    gen_adldata.cc \
    ../nukedopl3.c \
    ../dbopl.cpp \
    progs_cache.cpp \
    measurer.cpp

