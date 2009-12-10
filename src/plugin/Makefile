
PLUGIN_OBJS	=
PLUGIN_DFILES	= Makefile $(PLUGIN_OBJS:.o=.c) xmms.c bmp.c audacious.c \
		  winamp.c resource.rc resource.h in2.h out.h
PLUGIN_PATH	= src/plugin

dist-plugin:
	mkdir -p $(DIST)/$(PLUGIN_PATH)
	cp -RPp $(addprefix $(PLUGIN_PATH)/,$(PLUGIN_DFILES)) $(DIST)/$(PLUGIN_PATH)


WINDRES = i586-mingw32msvc-windres


plugin-audacious: $(PLUGIN_PATH)/xmp-audacious.so

$(PLUGIN_PATH)/xmp-audacious.so: $(PLUGIN_PATH)/audacious.lo $(LOBJS) $(D_LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) $(D_LOBJS) $(PLUGIN_PATH)/audacious.lo `pkg-config --libs audacious`'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(PLUGIN_PATH)/audacious.lo: $(PLUGIN_PATH)/audacious.c $(PLUGIN_PATH)/Makefile
	@CMD='$(CC) $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags audacious` -o $@ $(PLUGIN_PATH)/audacious.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-plugin-audacious:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir audacious`"; \
	echo "Installing xmp-audacious.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL_PROGRAM) $(PLUGIN_PATH)/xmp-audacious.so "$$DIR"



plugin-bmp: $(PLUGIN_PATH)/xmp-bmp.so

$(PLUGIN_PATH)/xmp-bmp.so: $(PLUGIN_PATH)/bmp.lo $(LOBJS) $(D_LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) $(D_LOBJS) $(PLUGIN_PATH)/bmp.lo `pkg-config --libs bmp`'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(PLUGIN_PATH)/bmp.lo: $(PLUGIN_PATH)/bmp.c $(PLUGIN_PATH)/Makefile
	@CMD='$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `pkg-config --cflags bmp` -o $@ $(PLUGIN_PATH)/bmp.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-plugin-bmp:
	@DIR="$(DESTDIR)`pkg-config --variable=input_plugin_dir bmp`"; \
	echo "Installing xmp-bmp.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL_PROGRAM) $(PLUGIN_PATH)/xmp-bmp.so "$$DIR"



plugin-xmms: $(PLUGIN_PATH)/xmp-xmms.so

$(PLUGIN_PATH)/xmp-xmms.so: $(PLUGIN_PATH)/xmms.lo $(LOBJS) $(D_LOBJS)
	@CMD='$(LD) -shared -o $@ $(LOBJS) $(D_LOBJS) $(PLUGIN_PATH)/xmms.lo `xmms-config --libs`'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(PLUGIN_PATH)/xmms.lo: $(PLUGIN_PATH)/xmms.c $(PLUGIN_PATH)/Makefile
	@CMD='$(CC) -c $(CFLAGS) -fPIC -D_REENTRANT $(XCFLAGS) `xmms-config --cflags` -o $@ $(PLUGIN_PATH)/xmms.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

install-plugin-xmms:
	@DIR="$(DESTDIR)`xmms-config --input-plugin-dir`"; \
	echo "Installing xmp-xmms.so in $$DIR"; \
	[ -d "$$DIR" ] || mkdir -p "$$DIR"; \
	$(INSTALL_PROGRAM) $(PLUGIN_PATH)/xmp-xmms.so "$$DIR"


# A hack to make this link correctly. It should be D_LOBJS but smix.lo
# fails to link (undefined _GLOBAL_OFFSET_TABLE_)
WIN_D_LOBJS = src/drivers/smix.o

plugin-winamp: $(PLUGIN_PATH)/in_xmp.dll

$(PLUGIN_PATH)/in_xmp.dll: $(PLUGIN_PATH)/winamp.lo $(PLUGIN_PATH)/resource.lo $(LOBJS) $(WIN_D_LOBJS)
	@CMD='$(LD) -shared -mwindows -o $@ $(LOBJS) $(WIN_D_LOBJS) $(PLUGIN_PATH)/winamp.lo $(PLUGIN_PATH)/resource.lo'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo LD $@ ; fi; \
	eval $$CMD

$(PLUGIN_PATH)/winamp.lo: $(PLUGIN_PATH)/winamp.c $(PLUGIN_PATH)/Makefile
	@CMD='$(CC) $(CFLAGS) $(XCFLAGS) -o $@ $(PLUGIN_PATH)/winamp.c'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $@ ; fi; \
	eval $$CMD

$(PLUGIN_PATH)/resource.lo: $(PLUGIN_PATH)/resource.rc $(PLUGIN_PATH)/resource.h
	$(WINDRES) -o$@ $(PLUGIN_PATH)/resource.rc

