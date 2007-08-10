# Extended Module Player src/drivers/Makefile
# $Id: Makefile,v 1.7 2007-08-10 20:02:16 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= file.o callback.o
DRV	= file.c oss_mix.c hpux.c bsd.c solaris.c oss_seq.c alsa.c alsa05.c \
	  esd.c sgi.c os2_dart.c netbsd.c openbsd.c aix.c nas.c arts.c qnx.c \
	  win32.c callback.c wav.c osx.c
DFILES	= Makefile $(DRV) debug.c
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	=
DCFILES	=
DRVDIR	= $(TOPDIR)/lib/drivers

all: $(LIB) library

include $(TOPDIR)/Makefile.rules

library: $(LIB)($(DRIVERS))

$(DRV:.c=.o): Makefile

arts.o: arts.c
	$(CC) $(CFLAGS) $(XCFLAGS) `artsc-config --cflags` -o $@ arts.c

arts.so: arts.o
	ld -shared -o $@ $+ -lartsc

esd.so: esd.o
	ld -shared -o $@ $+ -lesd
	
nas.so: nas.o
	ld -shared -o $@ $+ -laudio $(X_LIB) -lXt -lX11
	
include depend

