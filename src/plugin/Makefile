
PLUGIN_OBJS	= driver.o
PLUGIN_DFILES	= Makefile $(OBJS:.o=.c) xmms.c bmp.c audacious.c winamp.c \
		  resource.rc resource.h in2.h out.h

WINDRES = i586-mingw32msvc-windres


src/plugin/audacious.lo: src/plugin/audacious.c src/plugin/Makefile
	@if [ "$(V)" -gt 0 ]; then \
	    echo $(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags audacious` -o $@ src/plugin/audacious.c; \
	else echo "CC(fPIC)" src/plugin/audacious.c ; fi
	@$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags audacious` -o $@ src/plugin/audacious.c


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
	$(CC) $(CFLAGS) $(XCFLAGS) -o $@ winamp.c

resource.o: resource.rc resource.h
	$(WINDRES) -o$@ resource.rc


$(OBJS): Makefile

include depend

