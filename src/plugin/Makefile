
PLUGIN_OBJS	= driver.o
PLUGIN_DFILES	= Makefile $(PLUGIN_OBJS:.o=.c) xmms.c bmp.c audacious.c \
		  winamp.c resource.rc resource.h in2.h out.h
PLUGIN_PATH	= src/plugin

dist-plugin:
	mkdir -p $(DIST)/$(PLUGIN_PATH)
	cp -dp $(addprefix $(PLUGIN_PATH)/,$(PLUGIN_DFILES)) $(DIST)/$(PLUGIN_PATH)


WINDRES = i586-mingw32msvc-windres


audacious: src/plugin/xmp-audacious.so

src/plugin/xmp-audacious.so: src/plugin/audacious.lo $(LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) src/plugin/audacious.lo `pkg-config --libs audacious`'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

src/plugin/audacious.lo: src/plugin/audacious.c src/plugin/Makefile
	@CMD='$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags audacious` -o $@ src/plugin/audacious.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-audacious:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir audacious`"; \
	echo "Installing xmp-audacious.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 src/plugin/xmp-audacious.so "$$DIR"



bmp: src/plugin/xmp-bmp.so

src/plugin/xmp-bmp.so: src/plugin/bmp.lo $(LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) src/plugin/bmp.lo `pkg-config --libs bmp`'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

src/plugin/bmp.lo: src/plugin/bmp.c src/plugin/Makefile
	@CMD='$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags bmp` -o $@ src/plugin/bmp.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-bmp:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir bmp`"; \
	echo "Installing xmp-bmp.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 src/plugin/xmp-bmp.so "$$DIR"



xmms: src/plugins/xmp-xmms.so

src/plugins/xmp-xmms.so: src/plugin/xmms.lo $(LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) src/plugin/xmms.lo `xmms-config --libs`'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

src/plugin/xmms.lo: src/plugin/xmms.c src/plugin/Makefile
	@CMD='$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `xmms-config --cflags` -o $@ src/plugin/xmms.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-xmms:
	@DIR="$(DESTDIR)`xmms-config --input-plugin-dir`"; \
	echo "Installing xmp-xmms.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL) -s -m755 src/plugin/xmp-xmms.so "$$DIR"



winamp: src/plugins/in_xmp.dll

src/plugin/in_xmp.dll: src/plugin/winamp.c src/plugin/resource.rc src/plugin/Makefile
	@CMD='$(LD) -shared -mwindows -o $@ driver.o winamp.o resource.o'; \
	if [ "$(V)" -gt 0 ]; then $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

#winamp.o: winamp.c
#	$(CC) $(CFLAGS) $(XCFLAGS) -o $@ winamp.c

resource.o: resource.rc resource.h
	$(WINDRES) -o$@ resource.rc


include depend

