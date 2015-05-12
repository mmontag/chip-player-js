########################
#
# VGMPlay Makefile
# (for GNU Make 3.81)
#
########################

# Uncomment if you build on Windows using MinGW.
WINDOWS = 1

CC = gcc
CPP = g++
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

ifdef WINDOWS
# MinGW defines __WINDOWS__, Visual Studio defines WIN32
MAINFLAGS += -DWIN32 -D _WIN32_WINNT=0x500
endif

#MAINFLAGS += -D__BIG_ENDIAN__

# -- General Compile Flags --
CFLAGS := -O3 -g0 $(CFLAGS) -I. -Ilibs/include_mingw

ifdef WINDOWS
# for Windows, add kernel32 and winmm (Multimedia APIs)
LDFLAGS += -lkernel32 -lwinmm -ldsound -luuid -lole32
else
# for Linux, add librt (clock stuff) and libpthread (threads)
LDFLAGS += -lrt -lpthread -pthread
MAINFLAGS += -pthread -DSHARE_PREFIX=\"$(PREFIX)\"
endif

ifdef USE_LIBAO
LDFLAGS += -lao
endif

# add Library Path, if defined
ifdef LD_LIBRARY_PATH
LDFLAGS += -L $(LD_LIBRARY_PATH)
endif

SRC = .
OBJ = obj
AUDIOSRC = $(SRC)/audio
AUDIOOBJ = $(OBJ)/audio

OBJDIRS = \
	$(OBJ) \
	$(AUDIOOBJ)
MAINOBJS = \
	$(OBJ)/main.o
AUDIOOBJS = \
	$(AUDIOOBJ)/AudioStream.o \
	$(AUDIOOBJ)/AudDrv_WaveWriter.o \
	$(AUDIOOBJ)/AudDrv_WinMM.o \
	$(AUDIOOBJ)/AudDrv_DSound.o \
	$(AUDIOOBJ)/AudDrv_XAudio2.o


all:	audiotest

audiotest:	$(AUDIOOBJS) $(MAINOBJS)
	@echo Linking audiotest ...
	@$(CC) $(VGMPLAY_OBJS) $(MAINOBJS) $(AUDIOOBJS) $(LDFLAGS) -o audiotest
	@echo Done.

# compile the chip-emulator c-files
$(AUDIOOBJ)/%.o:	$(AUDIOSRC)/%.c
	@echo Compiling $< ...
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(MAINFLAGS) -c $< -o $@

$(AUDIOOBJ)/%.o:	$(AUDIOSRC)/%.cpp
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
	@rm -f $(MAINOBJS) $(AUDIOOBJS)
	@echo Deleting executable files ...
	@rm -f audiotest
	@echo Done.

#.PHONY: all clean install uninstall
