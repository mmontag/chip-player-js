
CC = gcc
CFLAGS = -O3 -Wall -I../include
LD = gcc
LDFLAGS =
LIBS = -L../lib -lxmp -lm

all: examples

examples: test1 test2 test3 test4 test5

test1: test1.o alsa.o
	$(LD) -o $@ $(LDFLAGS) test1.o alsa.o -lasound $(LIBS)

test2: test2.o alsa.o
	$(LD) -o $@ $(LDFLAGS) test2.o alsa.o -lasound $(LIBS)

test3: test3.o
	$(LD) -o $@ $(LDFLAGS) test3.o $(LIBS)

test4: test4.o
	$(LD) -o $@ $(LDFLAGS) test4.o $(LIBS)

test5: test5.o alsa.o
	$(LD) -o $@ $(LDFLAGS) test5.o alsa.o -lasound $(LIBS)

