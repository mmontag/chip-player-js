TEMPLATE=app
CONFIG-=qt
CONFIG+=console

CONFIG -= c++11

TARGET = adlmidi2
DESTDIR=$$PWD/../../bin/

#INCLUDEPATH += $$PWD/AudioCodecs/build/install/include
#LIBS += -L$$PWD/AudioCodecs/build/install/lib
INCLUDEPATH += $$PWD/../../src $$PWD/../../include
#LIBS += -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -lpthread -ldl
LIBS += -lSDL2 -lpthread -ldl

linux-*: {
    QMAKE_CXXFLAGS += -fopenmp
    QMAKE_LFLAGS += -fopenmp
}

#DEFINES += DEBUG_TIME_CALCULATION
#DEFINES += DEBUG_SEEKING_TEST
#DEFINES += DISABLE_EMBEDDED_BANKS
#DEFINES += ADLMIDI_USE_DOSBOX_OPL
#DEFINES += ENABLE_BEGIN_SILENCE_SKIPPING

QMAKE_CFLAGS    += -std=c90 -pedantic
QMAKE_CXXFLAGS  += -std=c++98 -pedantic

HEADERS += \
    $$PWD/../../include/adlmidi.h \
    $$PWD/../../include/adlmidi.hpp \
    $$PWD/../../src/adlbank.h \
    $$PWD/../../src/adldata.hh \
    $$PWD/../../src/adlmidi_mus2mid.h \
    $$PWD/../../src/adlmidi_private.hpp \
    $$PWD/../../src/adlmidi_xmi2mid.h \
    $$PWD/../../src/fraction.h \
    $$PWD/../../src/nukedopl3.h \
    $$PWD/../../src/dbopl.h \
    $$PWD/../../src/midiplay/wave_writer.h \
    6x9.hpp \
    8x16.hpp \
    9x15.hpp \
    puzzlegame.hpp

SOURCES += \
    $$PWD/../../src/adldata.cpp \
    \
    $$PWD/../../src/adlmidi.cpp \
    $$PWD/../../src/adlmidi_load.cpp \
    $$PWD/../../src/adlmidi_midiplay.cpp \
    $$PWD/../../src/adlmidi_mus2mid.c \
    $$PWD/../../src/adlmidi_opl3.cpp \
    $$PWD/../../src/adlmidi_private.cpp \
    $$PWD/../../src/adlmidi_xmi2mid.c \
    $$PWD/../../src/nukedopl3.c \
    $$PWD/../../src/dbopl.cpp \
    \
    midiplay.cc \
    puzzlegame.cc

