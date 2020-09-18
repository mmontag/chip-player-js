#-------------------------------------------------
#
# Project created by QtCreator 2012-12-26T16:54:29
#
#-------------------------------------------------

QT       -= core gui

TARGET = psflib
TEMPLATE = lib
CONFIG += staticlib

SOURCES += \
    psflib.c \
    psf2fs.c

HEADERS += psflib.h \
    psf2fs.h
unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
