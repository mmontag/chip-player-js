# Extended Module Player src/misc/Makefile

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= control.o info.o readdata.o load.o ppdepack.o unsqsh.o \
	  mmcmp.o readrle.o readhuff.o readlzw.o unarc.o arcfs.o \
	  oxm.o mkstemp.o xfd.o
DFILES	= Makefile $(OBJS:.o=.c)
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= $(BINS)
DCFILES	=

all: $(LIB)

include $(TOPDIR)/Makefile.rules

include depend

