# Extended Module Player src/bmp/Makefile
# $Id: Makefile,v 1.9 2007-09-13 01:42:42 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -I. -DVERSION=\"$(VERSION)\" $(PLUGFLAGS) -D_REENTRANT -DLIBDIR='"$(LIB_DIR)"' #-D_DEBUG
DFILES	= Makefile README xmp-plugin.h xpanel.h xstuff.c bg2.xpm $(OBJS:.o=.c)
CFILES	= xmp-plugin.so
INSTDIR	= $(DESTDIR)$(PLUGINST)
OBJS	= plugin.o driver.o xpanel.o fft4g.o font1.o font2.o


default: all

include $(TOPDIR)/Makefile.rules

all:
	@[ "x$(PLUGDIR)" = "xbmp" ] && $(MAKE) all-bmp || :

all-bmp: plugin

plugin: $(OBJS)
	$(LD) -shared -o xmp-plugin.so $(OBJS) -L../../lib/ -lxmp

dyndrv:
	make -C $(TOPDIR)/lib shared

install::
	@if [ "x$(PLUGDIR)" = "xbmp" ]; then \
		[ -d $(INSTDIR) ] || mkdir -p $(INSTDIR); \
		$(INSTALL) -s -m755 xmp-plugin.so $(INSTDIR); \
	fi

$(OBJS): Makefile

include depend

