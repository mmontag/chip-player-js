# Extended Module Player src/bmp/Makefile
# $Id: Makefile,v 1.11 2007-11-18 20:05:22 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -I. -DVERSION=\"$(VERSION)\" -D_REENTRANT -fPIC \
	  -DENABLE_PLUGIN
DFILES	= Makefile xpanel.h xstuff.c bg2.xpm $(OBJS:.o=.c) xmms.c bmp.c \
	  audacious.c winamp.c
CFILES	= xmp-audacious.so xmp-bmp.so xmp-xmms.so in_xmp.dll
OBJS	= driver.o xpanel.o fft4g.o font1.o font2.o

WINAMP_SDK = $(TOPDIR)/wa502_sdk/winamp
WINDRES = i586-mingw32msvc-windres

default: all

include $(TOPDIR)/Makefile.rules

all: $(PLUGINS)

install:: $(addprefix install-, $(PLUGINS))

	
#
# Audacious
#

audacious: xmp-audacious.so $(OBJS)
	@true

xmp-audacious.so: $(OBJS) audacious.o $(TOPDIR)/lib/libxmp-pic.a
	$(LD) -shared -o $@ $(OBJS) audacious.o `pkg-config --libs audacious` -L$(TOPDIR)/lib/ -lxmp-pic

audacious.o: audacious.c
	$(CC) -c $(CFLAGS) $(XCFLAGS) `pkg-config --cflags audacious` -o $@ audacious.c

install-audacious:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir audacious`"; \
	echo "Installing xmp-audacious.so in $$DIR..."; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 xmp-audacious.so "$$DIR"


#
# BMP
#

bmp: xmp-bmp.so $(OBJS)
	@true

xmp-bmp.so: $(OBJS) bmp.o $(TOPDIR)/lib/libxmp-pic.a
	$(LD) -shared -o $@ $(OBJS) bmp.o `pkg-config --libs bmp` -L$(TOPDIR)/lib/ -lxmp-pic

bmp.o: bmp.c
	$(CC) -c $(CFLAGS) $(XCFLAGS) `pkg-config --cflags bmp` -o $@ bmp.c

install-bmp:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir bmp`"; \
	echo "Installing xmp-bmp.so in $$DIR..."; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 xmp-bmp.so "$$DIR"

#
# XMMS
#

xmms: xmp-xmms.so $(OBJS)
	@true

xmp-xmms.so: $(OBJS) xmms.o $(TOPDIR)/lib/libxmp-pic.a
	$(LD) -shared -o $@ $(OBJS) xmms.o `xmms-config --libs` -L$(TOPDIR)/lib/ -lxmp-pic

xmms.o: xmms.c
	$(CC) -c $(CFLAGS) $(XCFLAGS) `xmms-config --cflags` -o $@ xmms.c

install-xmms:
	@DIR="$(DESTDIR)`xmms-config --input-plugin-dir`"; \
	echo "Installing xmp-xmms.so in $$DIR..."; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 xmp-xmms.so "$$DIR"

#
# Winamp
#

winamp: in_xmp.dll
	@true

in_xmp.dll: driver.o winamp.o resource.o $(TOPDIR)/lib/libxmp-pic.a resource.h
	$(LD) -shared -mwindows -o $@ driver.o winamp.o resource.o -L$(TOPDIR)/lib/ -lxmp-pic

winamp.o: winamp.c
	$(CC) $(CFLAGS) $(XCFLAGS) -I$(WINAMP_SDK) -o $@ winamp.c

resource.o: resource.rc resource.h
	$(WINDRES) -o$@ resource.rc


$(OBJS): Makefile

include depend

