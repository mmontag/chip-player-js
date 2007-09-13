# Extended Module Player src/main/Makefile
# $Id: Makefile,v 1.9 2007-09-13 00:45:05 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= getopt.o getopt1.o options.o
XDEPS	=
BINS	= xmp
XLIBS	= -L$(TOPDIR)/lib -lxmp -lm #-lefence
DFILES	= Makefile $(OBJS:.o=.c) main.c simple.c mod2mp3.c
DDIRS	=
CFILES	= $(BINS)
DCFILES	=

all: bins

include $(TOPDIR)/Makefile.rules

bins: $(BINS)

dyndrv:
	make -C $(TOPDIR)/lib shared

xmp: $(OBJS) main.o ../../lib/libxmp.a
	$(LD) $(LDFLAGS) $(OBJS) main.o $(LIBS)

install:: $(BINS)
	@echo Installing binaries in $(BIN_DIR)...
	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
	$(INSTALL) -s -m755 $(BINS) $(BIN_DIR)

$(OBJS): $(TOPDIR)/Makefile.rules Makefile

simple: simple.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

mod2mp3: mod2mp3.o
	$(LD) $(LDFLAGS) $+ $(LIBS)

clean::
	rm -f simple

include depend
