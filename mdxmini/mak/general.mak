#
# unix
#

CC = gcc
LD = gcc
AR = ar
LIBS = 
SLIBS =
LFLAGS =
 
ifdef DEBUG
CFLAGS = -g -O0
OBJDIR = obj_dbg
else
CFLAGS = -g -O3
OBJDIR = obj
endif

# iconv
ifneq ($(OS),Windows_NT)
CFLAGS += -DUSE_ICONV
LIBS += -liconv
endif

#
# SDL stuff
#

SDL_CONFIG = sdl-config

SDL_SLIBS  := `$(SDL_CONFIG) --static-libs`
SDL_LIBS   := `$(SDL_CONFIG) --libs`
SDL_CFLAGS := `$(SDL_CONFIG) --cflags`
