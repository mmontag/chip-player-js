# Extended Module Player src/misc/Makefile

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= control.o info.o ipc.o load.o ppdepack.o unsqsh.o crc32.o mmcmp.o \
	  readrle.o readhuff.o readlzw.o unarc.o
XDEPS	=
XLIBS	= -L$(TOPDIR)/lib -lxmp
DFILES	= Makefile $(OBJS:.o=.c)
DDIRS	=
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	= $(BINS)
DCFILES	=

all: $(LIB)

include $(TOPDIR)/Makefile.rules

$(BINS): $(TOPDIR)/lib/libxmp.a
	$(CC) $(CFLAGS) $(XCFLAGS) $@.c
	$(LD) $(LFLAGS) $@.o $(LIBS)

include depend

