# Extended Module Player src/bmp/Makefile
# $Id: Makefile,v 1.4 2007-08-21 17:16:03 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -I../main -DVERSION=\"$(VERSION)\" `pkg-config bmp --cflags` -DXMMS_PLUGIN -DBMP_PLUGIN -D_REENTRANT -DLIBDIR='"$(LIB_DIR)"' #-D_DEBUG
DFILES	= Makefile README xmp-plugin.h $(OBJS:.o=.c)
CFILES	= xmp-plugin.so
INSTDIR	= $(DESTDIR)`pkg-config bmp --input-plugin-dir`
LINKS	= xpanel.c fft4g.c font1.c font2.c
OBJS	= plugin.o driver.o $(LINKS:.c=.o)


default: all

include $(TOPDIR)/Makefile.rules

all:
	@[ "x$(PLUGIN)" = "xbmp" ] && $(MAKE) all-bmp || :

all-bmp: driver.c plugin

plugin: $(OBJS)
	$(LD) -shared -o xmp-plugin.so $(OBJS) -L../../lib/ -lxmp

dyndrv:
	make -C $(TOPDIR)/lib shared

driver.c:
	ln -sf ../player/driver.c .

$(LINKS):
	ln -sf ../main/$@ .

install::
	@[ -d $(INSTDIR) ] || mkdir -p $(INSTDIR)
	$(INSTALL) -s -m755 xmp-plugin.so $(INSTDIR)

$(OBJS): Makefile

depend: driver.c $(LINKS)

include depend

