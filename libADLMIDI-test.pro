TEMPLATE=app
CONFIG-=qt
CONFIG+=console

TARGET=adlmidiplay
DESTDIR=$$PWD/bin/

#INCLUDEPATH += $$PWD/AudioCodecs/build/install/include
#LIBS += -L$$PWD/AudioCodecs/build/install/lib
INCLUDEPATH += $$PWD/src
#LIBS += -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -lpthread -ldl
LIBS += -lSDL2 -lpthread -ldl

#DEFINES += DISABLE_EMBEDDED_BANKS

HEADERS += \
    src/adlbank.h \
    src/adldata.hh \
    src/adlmidi.h \
    src/adlmidi_mus2mid.h \
    src/adlmidi_private.hpp \
    src/adlmidi_xmi2mid.h \
    src/fraction.h \
    src/nukedopl3.h \
    src/midiplay/wave_writer.h

SOURCES += \
    src/adldata.cpp \
    \
    src/adlmidi.cpp \
    src/adlmidi_load.cpp \
    src/adlmidi_midiplay.cpp \
    src/adlmidi_mus2mid.c \
    src/adlmidi_opl3.cpp \
    src/adlmidi_private.cpp \
    src/adlmidi_xmi2mid.c \
    src/nukedopl3.c \
    src/midiplay/adlmidiplay.cpp \
    src/midiplay/wave_writer.c
