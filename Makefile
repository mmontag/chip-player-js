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
ARFLAGS = -cr

# add Library Path, if defined
ifdef LD_LIBRARY_PATH
LDFLAGS += -L $(LD_LIBRARY_PATH)
endif

#MAINFLAGS += -D__BIG_ENDIAN__


ifeq ($(WINDOWS), 1)
# MinGW defines __WINDOWS__, Visual Studio defines WIN32
# also assume Windows 2000 and later for GetConsoleWindow API call
MAINFLAGS += -DWIN32 -D _WIN32_WINNT=0x500
endif

ifeq ($(WINDOWS), 1)
# for Windows, add kernel32 and winmm (Multimedia APIs)
LDFLAGS += -lkernel32 -lwinmm -ldsound -luuid -lole32
else
# for Linux, add librt (clock stuff) and libpthread (threads)
LDFLAGS += -lrt -lpthread -pthread
MAINFLAGS += -pthread -DSHARE_PREFIX=\"$(PREFIX)\"
endif

SRC = .
OBJ = obj
LIBAUDSRC = $(SRC)/audio
LIBAUDOBJ = $(OBJ)/audio

OBJDIRS = \
	$(OBJ) \
	$(LIBAUDOBJ)

ALL_LIBS = \
	$(LIBAUD_A)

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
endif

ifneq ($(WINDOWS), 1)
ifneq ($(USE_BSD_AUDIO), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_OSS.o
else
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_SADA.o
endif

ifeq ($(USE_ALSA), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_ALSA.o
LDFLAGS += -lasound
endif
endif

ifeq ($(USE_LIBAO), 1)
LIBAUDOBJS += \
	$(LIBAUDOBJ)/AudDrv_libao.o
LDFLAGS += -lao
endif



all:	audiotest

audiotest:	libaudio $(AUD_MAINOBJS)
	@echo Linking audiotest ...
	@$(CC) $(AUD_MAINOBJS) $(LIBAUD_A) $(LDFLAGS) -o audiotest
	@echo Done.

libaudio:	$(LIBAUDOBJS)
	@echo Archiving libaudio.a ...
	@$(RM) $@
	@$(AR) $(ARFLAGS) $(LIBAUD_A) $(LIBAUDOBJS)

# compile the audio library c-files
$(LIBAUDOBJ)/%.o:	$(LIBAUDSRC)/%.c
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(MAINFLAGS) -c $< -o $@

$(LIBAUDOBJ)/%.o:	$(LIBAUDSRC)/%.cpp
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) $(MAINFLAGS) -c $< -o $@

# compile the main c-files
$(OBJ)/%.o:	$(SRC)/%.c
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(MAINFLAGS) -c $< -o $@

clean:
	@echo Deleting object files ...
	@rm -f $(AUD_MAINOBJS) $(ALL_LIBS) $(LIBAUDOBJS)
	@echo Deleting executable files ...
	@rm -f audiotest
	@echo Done.

#.PHONY: all clean install uninstall
