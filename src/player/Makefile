# Extended Module Player src/player/Makefile
# $Id: Makefile,v 1.10 2007-10-31 22:49:03 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -DSYSCONFDIR=\"$(SYSCONFDIR)\" -I../include
OBJS	= convert.o driver.o formats.o misc.o period.o player.o \
	  readrc.o scan.o ulaw.o cruncher.o med_synth.o filter.o \
	  fmopl.o synth.o am_synth.o
XOBJS	= version.o $(MIXER)
XDEPS	=
DFILES	= Makefile $(OBJS:.o=.c) effects.c mixer.c player.h mix_all.c \
	  fmopl.h synth.h
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= version.c $(DRIVERS)
DCFILES	=

all: $(LIB)

include $(TOPDIR)/Makefile.rules

version.c: $(DFILES) $(TOPDIR)/Makefile.rules
	@echo "Creating version id..."; \
	printf "char *xmp_version = \"$(VERSION)\";\n">$@; \
	printf "char *xmp_date = \"$(DATE)\";\n">>$@; \
	printf "char *xmp_build=\"Compiled by `whoami`@`hostname` `date` (`uname -sr`)\";\n">>$@

include depend
