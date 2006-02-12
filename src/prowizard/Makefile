# Extended Module Player src/player/Makefile
# $Id: Makefile,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $

XCFLAGS	= -I../include -DLIBDIR='"$(LIB_DIR)"'
OBJS	= prowiz.o ptktable.o tuning.o \
	  ac1d.o di.o eureka.o fc-m.o fuchs.o fuzzac.o heatseek.o kris.o \
	  ksm.o mp.o np1.o np2.o np3.o p60a.o pm10c.o pm18a.o pha.o prun1.o \
	  prun2.o qc.o stim.o tdd.o unic.o unic2.o wn.o xann.o zen.o
XOBJS	=
XDEPS	=
DFILES	= Makefile $(OBJS:.o=.c)
DDIRS	=
LIB	= ../../lib/libxmp.a
CFILES	= $(LIB) version.c $(DRIVERS)
DCFILES	=

all: $(LIB)

include ../../Makefile.rules

$(LIB): $(OBJS)

include depend
