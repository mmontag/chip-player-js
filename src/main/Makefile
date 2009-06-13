# Extended Module Player src/main/Makefile

TOPDIR	= ../..
XCFLAGS	= -I../include
OBJS	= getopt.o getopt1.o options.o
XDEPS	= main.c
BINS	= xmp
XLIBS	= -L$(TOPDIR)/lib -lxmp $(LIBM)
DFILES	= Makefile $(OBJS:.o=.c) main.c
DDIRS	=
CFILES	= $(BINS)
DCFILES	=

all: bins

include $(TOPDIR)/Makefile.rules

bins: $(BINS)

xmp: $(OBJS) main.o $(TOPDIR)/lib/libxmp.a
	$(LD) $(LDFLAGS) $(OBJS) main.o $(LIBS)

install:: $(BINS)
	@echo Installing binaries in $(DESTDIR)$(BINDIR)...
	@[ -d $(DESTDIR)$(BINDIR) ] || mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -s -m755 $(BINS) $(DESTDIR)$(BINDIR)

$(OBJS): $(TOPDIR)/Makefile.rules Makefile

include depend
