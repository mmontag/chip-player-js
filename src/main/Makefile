# Extended Module Player src/main/Makefile
# $Id: Makefile,v 1.15 2007-10-15 19:19:22 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= getopt.o getopt1.o options.o
XDEPS	=
BINS	= xmp
XLIBS	= -L$(TOPDIR)/lib -lxmp $(LIBM)
DFILES	= Makefile $(OBJS:.o=.c) main.c simple.c mod2mp3.c xstuff.c
DDIRS	=
CFILES	= $(BINS)
DCFILES	=

all: bins

include $(TOPDIR)/Makefile.rules

bins: $(BINS)

dyndrv:
	make -C $(TOPDIR)/lib shared

xmp: $(OBJS) main.o ../../lib/libxmp.a
	$(LD) $(LDFLAGS) $(OBJS) main.o $(LIBS) #-lefence

install:: $(BINS)
	@echo Installing binaries in $(DESTDIR)$(BINDIR)...
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -s -m755 $(BINS) $(DESTDIR)$(BINDIR)

$(OBJS): $(TOPDIR)/Makefile.rules Makefile

simple: simple.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

mod2mp3: mod2mp3.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

clean::
	rm -f simple

include depend
