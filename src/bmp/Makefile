# Extended Module Player src/bmp/Makefile
# $Id: Makefile,v 1.13 2007-09-25 12:45:04 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -I. -DVERSION=\"$(VERSION)\" $(PLUGFLAGS) -D_REENTRANT
DFILES	= Makefile xpanel.h xstuff.c bg2.xpm $(OBJS:.o=.c)
CFILES	= xmp-plugin.so
INSTDIR	= $(DESTDIR)$(PLUGINST)
OBJS	= plugin.o driver.o xpanel.o fft4g.o font1.o font2.o
PLUGIN	= xmp-plugin.so


default: all

include $(TOPDIR)/Makefile.rules

all:
	@[ "x$(PLUGDIR)" = "xbmp" ] && $(MAKE) all-bmp || :

all-bmp: $(PLUGIN)

$(PLUGIN): $(OBJS) $(TOPDIR)/lib/libxmp-pic.a
	$(LD) -shared -o $@ $(OBJS) -L../../lib/ -lxmp-pic

install::
	@if [ "x$(PLUGDIR)" = "xbmp" ]; then \
		[ -d $(INSTDIR) ] || mkdir -p $(INSTDIR); \
		echo Install $(PLUGIN) in $(INSTDIR); \
		$(INSTALL) -s -m755 $(PLUGIN) $(INSTDIR); \
	fi

$(OBJS): Makefile

include depend

