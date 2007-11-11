# Extended Module Player src/drivers/Makefile
# $Id: Makefile,v 1.13 2007-11-11 16:06:52 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= file.o wav.o callback.o
DRV	= file.c oss_mix.c hpux.c bsd.c solaris.c oss_seq.c alsa.c alsa05.c \
	  esd.c sgi.c os2_dart.c netbsd.c openbsd.c aix.c nas.c arts.c qnx.c \
	  win32.c callback.c wav.c osx.c beos.c alsa_seq.c pulseaudio.c \
	  amiga.c
DFILES	= Makefile $(DRV) debug.c
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	=
DCFILES	=
DRVDIR	= $(TOPDIR)/lib/drivers

all: $(LIB) library

include $(TOPDIR)/Makefile.rules

beos.o: beos.c
	g++ -c $(XCFLAGS) -o beos.o beos.c

library: $(LIB)($(DRIVERS))

$(DRV:.c=.o): Makefile

arts.o: arts.c
	$(CC) $(CFLAGS) $(XCFLAGS) `artsc-config --cflags` -o $@ arts.c

include depend

