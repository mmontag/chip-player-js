TEMPLATE=app
CONFIG-=qt
CONFIG+=console

TARGET=adlmidiplay
DESTDIR=$$PWD/bin/

INCLUDEPATH += /home/vitaly/_git_repos/PGE-Project/_Libs/_builds/linux/include
INCLUDEPATH += $$PWD/src
LIBS += -L/home/vitaly/_git_repos/PGE-Project/_Libs/_builds/linux/lib
LIBS += -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -lpthread -ldl

HEADERS += \
    src/adlbank.h \
    src/adldata.hh \
    src/adlmidi.h \
    src/adlmidi_mus2mid.h \
    src/adlmidi_private.hpp \
    src/adlmidi_xmi2mid.h \
    src/fraction.h \
    src/nukedopl3.h

SOURCES += \
    src/adldata.cpp \
    src/adlmidi.cpp \
    src/adlmidi_load.cpp \
    src/adlmidi_midiplay.cpp \
    src/adlmidi_mus2mid.c \
    src/adlmidi_opl3.cpp \
    src/adlmidi_private.cpp \
    src/adlmidi_xmi2mid.c \
    src/midiplay/adlmidiplay.cpp \
    src/nukedopl3.c
