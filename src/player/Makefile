# Extended Module Player src/player/Makefile
# $Id: Makefile,v 1.2 2001-11-30 11:03:21 cmatsuoka Exp $

XCFLAGS	= -I../include -DLIBDIR='"$(LIB_DIR)"'
OBJS	= convert.o driver.o formats.o misc.o period.o player.o \
	  readrc.o scan.o ulaw.o med_synth.o cruncher.o filter.o \
	  fmopl.o synth.o
XOBJS	= version.o $(MIXER)
XDEPS	=
DFILES	= Makefile $(OBJS:.o=.c) effects.c mixer.c player.h mix_all.c \
	  fmopl.h synth.h
DDIRS	=
LIB	= ../../lib/libxmp.a
CFILES	= $(LIB) version.c $(DRIVERS)
DCFILES	=

all: $(LIB)

include ../../Makefile.rules

mix_i386.o: mix_i386.S

version.c: $(DFILES) ../../Makefile.rules
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
