# Extended Module Player src/prowizard/Makefile
# $Id: Makefile,v 1.7 2007-08-10 20:02:17 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -DLIBDIR='"$(LIB_DIR)"'
OBJS	= prowiz.o ptktable.o tuning.o \
	  ac1d.o di.o eureka.o fc-m.o fuchs.o fuzzac.o gmc.o heatseek.o \
	  kris.o ksm.o mp.o np1.o np2.o np3.o p60a.o pm10c.o pm18a.o pha.o \
	  prun1.o prun2.o tdd.o unic.o unic2.o wn.o zen.o
OBJS2	= gmc.o hrt.o noiserun.o p40.o p41a.o p50a.o p61a.o pm.o pm01.o \
	  pm20.o pm40.o pp10.o pp21.o pp30.o skyt.o starpack.o tp1.o \
	  tp2.o tp3.o ptk.o soundtk.o
DUPES	= qc.o xann.o stim.o
XOBJS	=
XDEPS	=
DFILES	= Makefile $(OBJS:.o=.c) $(OBJS2:.o=.c) $(DUPES:.o=.c) prowiz.h list.h
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= $(LIB) version.c $(DRIVERS)
DCFILES	=

all: $(LIB)

include $(TOPDIR)/Makefile.rules

$(LIB): $(OBJS)

include depend
