# Extended Module Player src/xmms/Makefile
# $Id: Makefile,v 1.7 2005-02-23 14:25:50 cmatsuoka Exp $

XCFLAGS	= -I../include -I../main -DVERSION=\"$(VERSION)\" `pkg-config bmp --cflags` -DBMP_PLUGIN -D_REENTRANT -DLIBDIR='"$(LIB_DIR)"' #-D_DEBUG
DFILES	= Makefile README $(OBJS:.o=.c)
CFILES	= xmp-plugin.so
INSTDIR	= $(DEST_DIR)`xmms-config --input-plugin-dir`
OBJS	= plugin.o xpanel.o fft4g.o font1.o font2.o


default: all

include ../../Makefile.rules

all:
	@[ "x$(XMMS)" != "x" ] && $(MAKE) all-xmms || :

all-xmms: plugin

plugin: $(OBJS) $(LINKS) $(DYNDRV)
	$(LD) -shared -o xmp-plugin.so $(OBJS) -L../../lib/ -lxmp

dyndrv:
	make -C ../../lib shared

install::
	@[ -d $(INSTDIR) ] || mkdir -p $(INSTDIR)
	$(INSTALL) -s -m755 xmp-plugin.so $(INSTDIR)

$(OBJS): Makefile

include depend

