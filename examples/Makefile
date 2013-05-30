
CC = gcc
CFLAGS = -O3 -Wall -I../include
LD = gcc
LDFLAGS =
LIBS = -L../lib -lxmp

all: examples

examples: player-simple player-showpatterns showinfo player-getbuffer test3

player-simple: player-simple.o alsa.o
	$(LD) -o $@ $(LDFLAGS) $+ -lasound $(LIBS)

player-showpatterns: player-showpatterns.o alsa.o
	$(LD) -o $@ $(LDFLAGS) $+ -lasound $(LIBS)

test3: test3.o
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

showinfo: showinfo.o
	$(LD) -o $@ $(LDFLAGS) $+ $(LIBS)

player-getbuffer: player-getbuffer.o alsa.o
	$(LD) -o $@ $(LDFLAGS) $+ -lasound $(LIBS)

player-openal: player-openal.o
	$(LD) -o $@ $(LDFLAGS) $+ -lopenal $(LIBS)
