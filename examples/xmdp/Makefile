
CC = gcc
CFLAGS = -O3 -Wall -I../../include
LD = gcc
LDFLAGS =
LIBS = -L../../lib -lxmp -lm

all: xmdp

xmdp: mdp.o font1.o font2.o ../alsa.o
	$(LD) -o $@ $(LDFLAGS) $+ -lSDL -lasound $(LIBS)
