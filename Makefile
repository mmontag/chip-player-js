########################
#
# libvgm Makefile
#
########################

ifeq ($(OS),Windows_NT)
WINDOWS = 1
else
WINDOWS = 0
endif

ifeq ($(WINDOWS), 0)
USE_BSD_AUDIO = 0
USE_ALSA = 1
USE_LIBAO = 1
endif

CC = gcc
CPP = g++
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CFLAGS := -O3 -g0 $(CFLAGS) -I. -Ilibs/include_mingw
CCFLAGS = -std=gnu99
CPPFLAGS = -std=gnu++98
#CFLAGS += -Wall -Wextra -Wpedantic
ARFLAGS = -cr

# add Library Path, if defined
ifdef LD_LIBRARY_PATH
LDFLAGS += -L $(LD_LIBRARY_PATH)
endif

#CFLAGS += -D__BIG_ENDIAN__


ifeq ($(WINDOWS), 1)
# assume Windows 2000 and later for GetConsoleWindow API call
CFLAGS += -D _WIN32_WINNT=0x500
endif

ifeq ($(WINDOWS), 1)
# for Windows, add kernel32 and winmm (Multimedia APIs)
LDFLAGS += -lkernel32 -lwinmm -ldsound -luuid -lole32
else
# for Linux add pthread support (-pthread should include -lpthread)
LDFLAGS += -lrt -pthread
CFLAGS += -pthread -DSHARE_PREFIX=\"$(PREFIX)\"

# add librt (clock stuff)
#LDFLAGS += -lrt
endif

SRC = .
OBJ = obj
LIBAUDSRC = $(SRC)/audio
LIBAUDOBJ = $(OBJ)/audio
LIBEMUSRC = $(SRC)/emu
LIBEMUOBJ = $(OBJ)/emu

OBJDIRS = \
	$(OBJ) \
	$(LIBAUDOBJ) \
	$(LIBEMUOBJ) \
	$(LIBEMUOBJ)/cores

ALL_LIBS = \
	$(LIBAUD_A) \
	$(LIBEMU_A)


#### Audio Output Library ####
AUD_MAINOBJS = \
	$(OBJ)/audiotest.o
LIBAUD_A = $(OBJ)/libaudio.a
LIBAUDOBJS = \
	$(LIBAUDOBJ)/AudioStream.o \
	$(LIBAUDOBJ)/AudDrv_WaveWriter.o

ifeq ($(WINDOWS), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_WinMM.o \
	$(LIBAUDOBJ)/AudDrv_DSound.o \
	$(LIBAUDOBJ)/AudDrv_XAudio2.o
	#$(LIBAUDOBJ)/AudDrv_WASAPI.o	# MinGW lacks the required header files
CFLAGS += -D AUDDRV_WINMM
CFLAGS += -D AUDDRV_DSOUND
CFLAGS += -D AUDDRV_XAUD2
#CFLAGS += -D AUDDRV_WASAPI
endif

ifneq ($(WINDOWS), 1)
ifneq ($(USE_BSD_AUDIO), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_OSS.o
CFLAGS += -D AUDDRV_OSS
else
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_SADA.o
CFLAGS += -D AUDDRV_SADA
endif

ifeq ($(USE_ALSA), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_ALSA.o
LDFLAGS += -lasound
CFLAGS += -D AUDDRV_ALSA
endif
endif

ifeq ($(USE_LIBAO), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_libao.o
LDFLAGS += -lao
CFLAGS += -D AUDDRV_LIBAO
endif


#### Sound Emulation Library ####
EMU_MAINOBJS = \
	$(OBJ)/emutest.o
LIBEMU_A = $(OBJ)/libemu.a
LIBEMUOBJS = \
	$(LIBEMUOBJ)/SoundEmu.o \
	$(LIBEMUOBJ)/cores/sn764intf.o \
	$(LIBEMUOBJ)/cores/sn76496.o \
	$(LIBEMUOBJ)/cores/sn76489.o \
	$(LIBEMUOBJ)/cores/2413intf.o \
	$(LIBEMUOBJ)/cores/emu2413.o \
	$(LIBEMUOBJ)/cores/ym2413.o \
	$(LIBEMUOBJ)/cores/okim6295.o \
	$(LIBEMUOBJ)/panning.o


AUDEMU_MAINOBJS = \
	$(OBJ)/audemutest.o



all:	audiotest emutest audemutest

audiotest:	dirs libaudio $(AUD_MAINOBJS)
	@echo Linking audiotest ...
	@$(CC) $(AUD_MAINOBJS) $(LIBAUD_A) $(LDFLAGS) -o audiotest
	@echo Done.

libaudio:	$(LIBAUDOBJS)
	@echo Archiving libaudio.a ...
	@$(AR) $(ARFLAGS) $(LIBAUD_A) $(LIBAUDOBJS)

emutest:	dirs libemu $(EMU_MAINOBJS)
	@echo Linking emutest ...
	@$(CC) $(EMU_MAINOBJS) $(LIBEMU_A) $(LDFLAGS) -o emutest
	@echo Done.

libemu:	$(LIBEMUOBJS)
	@echo Archiving libemu.a ...
	@$(AR) $(ARFLAGS) $(LIBEMU_A) $(LIBEMUOBJS)

audemutest:	dirs libaudio libemu $(AUDEMU_MAINOBJS)
	@echo Linking audemutest ...
	@$(CC) $(AUDEMU_MAINOBJS) $(LIBAUD_A) $(LIBEMU_A) $(LDFLAGS) -o audemutest
	@echo Done.


dirs:
	@mkdir -p $(OBJDIRS)

$(OBJ)/%.o: $(SRC)/%.c
	@echo Compiling $< ...
	@$(CC) $(CFLAGS) $(CCFLAGS) -c $< -o $@

$(OBJ)/%.o: $(SRC)/%.cpp
	@echo Compiling $< ...
	@$(CPP) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	@echo Deleting object files ...
	@rm -f $(AUD_MAINOBJS) $(ALL_LIBS) $(LIBAUDOBJS)
	@echo Deleting executable files ...
	@rm -f audiotest
	@echo Done.

#.PHONY: all clean install uninstall
