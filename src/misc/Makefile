# Extended Module Player src/misc/Makefile

XCFLAGS	= -I../include
OBJS	= control.o info.o ipc.o load.o depack.o unsqsh.o crc32.o mmcmp.o
XDEPS	=
XLIBS	= -L../../lib -lxmp
DFILES	= Makefile $(OBJS:.o=.c)
DDIRS	=
LIB	= ../../lib/libxmp.a
CFILES	= $(BINS)
DCFILES	=

all: $(LIB)

include ../../Makefile.rules

$(BINS): ../../lib/libxmp.a
	$(CC) $(CFLAGS) $(XCFLAGS) $@.c
	$(LD) $(LFLAGS) $@.o $(LIBS)

include depend

