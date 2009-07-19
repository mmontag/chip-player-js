
DRV	= file.c oss.c hpux.c bsd.c solaris.c oss_seq.c alsa.c alsa05.c \
	  esd.c sgi.c os2_dart.c netbsd.c openbsd.c aix.c nas.c arts.c qnx.c \
	  win32.c callback.c wav.c osx.c beos.c alsa_seq.c pulseaudio.c \
	  amiga.c sndio.c

DRIVERS_OBJS	= file.o wav.o callback.o $(DRIVERS)
DRIVERS_DFILES	= Makefile $(DRV) debug.c

beos.o: src/drivers/beos.c
	g++ -c $(XCFLAGS) -o beos.o $+

arts.o: src/drivers/arts.c
	$(CC) $(CFLAGS) $(XCFLAGS) `artsc-config --cflags` -o $@ $+

