# Extended Module Player src/main/Makefile
# $Id: Makefile,v 1.5 2005-02-10 13:36:26 cmatsuoka Exp $

XCFLAGS	= -I../include $(X_INC)
OBJS	= getopt.o getopt1.o options.o
X11OBJS	= xpanel.o xstuff.o font1.o font2.o fft4g.o
XDEPS	=
BINS	= xmp $(X_XMP) mod2mp3
XLIBS	= -L../../lib -lxmp -lm #-lefence
X11LIBS	= -lX11 -lXext
XPMS	= bg1.xpm bg2.xpm bg3.xpm
DFILES	= Makefile $(OBJS:.o=.c) $(X11OBJS:.o=.c) main.c xpanel.h $(XPMS) \
	  simple.c mod2mp3.c
DDIRS	=
CFILES	= $(BINS)
DCFILES	=

all: bins

include ../../Makefile.rules

bins: $(BINS)

dyndrv:
	make -C ../../lib shared

xmp: $(OBJS) main.o ../../lib/libxmp.a $(DYNDRV)
	$(LD) $(LDFLAGS) $(OBJS) main.o $(LIBS)

xxmp: $(OBJS) xmain.o $(X11OBJS) ../../lib/libxmp.a $(DYNDRV)
	$(LD) $(LDFLAGS) $(OBJS) $(X11OBJS) xmain.o $(X_LIB) $(X11LIBS) $(LIBS)

xmain.o: main.c xpanel.h
	$(CC) $(CFLAGS) $(XCFLAGS) -DXXMP -o xmain.o main.c

install:: $(BINS)
	@echo Installing binaries in $(BIN_DIR)...
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	$(INSTALL) -s -m755 $(BINS) $(BIN_DIR)

$(OBJS): ../../Makefile.rules Makefile

simple: simple.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

mod2mp3: mod2mp3.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

dump: dump.o
	$(LD) $(LDFLAGS) $+

clean::
	rm -f simple

include depend
