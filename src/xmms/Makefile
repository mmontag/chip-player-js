# Extended Module Player src/xmms/Makefile
# $Id: Makefile,v 1.6 2005-02-10 13:36:28 cmatsuoka Exp $

XCFLAGS	= -I../include -I../main -DVERSION=\"$(VERSION)\" `pkg-config bmp --cflags` -DXMMS_PLUGIN -DBMP_PLUGIN -D_REENTRANT -DLIBDIR='"$(LIB_DIR)"' #-D_DEBUG
DFILES	= Makefile README $(OBJS:.o=.c)
CFILES	= xmp-plugin.so
INSTDIR	= $(DEST_DIR)`xmms-config --input-plugin-dir`
LINKS	= xpanel.c fft4g.c font1.c font2.c
OBJS	= plugin.o driver.o $(LINKS:.c=.o)


default: all

include ../../Makefile.rules

all:
	@[ "x$(XMMS)" != "x" ] && $(MAKE) all-xmms || :

all-xmms: driver.c plugin

plugin: $(OBJS) $(LINKS) $(DYNDRV)
	$(LD) -shared -o xmp-plugin.so $(OBJS) -L../../lib/ -lxmp

dyndrv:
	make -C ../../lib shared

driver.c:
	ln -sf ../player/driver.c .

$(LINKS):
	ln -sf ../main/$@ .

install::
	@[ -d $(INSTDIR) ] || mkdir -p $(INSTDIR)
	$(INSTALL) -s -m755 xmp-plugin.so $(INSTDIR)

$(OBJS): Makefile

include depend

