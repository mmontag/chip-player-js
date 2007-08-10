# Extended Module Player src/player/Makefile
# $Id: Makefile,v 1.4 2007-08-10 20:02:17 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -DLIBDIR='"$(LIB_DIR)"'
OBJS	= convert.o driver.o formats.o misc.o period.o player.o \
	  readrc.o scan.o ulaw.o cruncher.o med_synth.o filter.o \
	  fmopl.o synth.o
XOBJS	= version.o $(MIXER)
XDEPS	=
DFILES	= Makefile $(OBJS:.o=.c) effects.c mixer.c player.h mix_all.c \
	  fmopl.h synth.h
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= $(LIB) version.c $(DRIVERS)
DCFILES	=

all: $(LIB)

include $(TOPDIR)/Makefile.rules

mix_i386.o: mix_i386.S

version.c: $(DFILES) $(TOPDIR)/Makefile.rules
	@echo "Creating version id..."; \
	printf "char *xmp_version = \"$(VERSION)\";\n">$@; \
	printf "char *xmp_date = \"$(DATE)\";\n">>$@; \
	printf "char *xmp_build=\"Compiled by `whoami`@`hostname` `date` (`uname -sr`)\";\n">>$@

server: ../bin/server
	@ls -l $+

../bin/server: server.o
	$(LD) $(LFLAGS) $+

$(LIB): $(OBJS)

include depend
