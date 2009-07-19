
VERSION	= 2.8.0-pre
DIST    = xmp-$(VERSION)
MODULES = "mod.syfuid.long"
DFILES  = README INSTALL configure configure.in Makefile Makefile.rules.in \
	  scripts $(MODULES)
DDIRS	= docs drivers etc include loaders misc player plugin prowiz win32 \
	  main

V	= 0

all: binaries

include Makefile.rules
include src/drivers/Makefile
include src/include/Makefile
include src/loaders/Makefile
include src/loaders/prowizard/Makefile
include src/misc/Makefile
include src/player/Makefile

ifneq ($(PLATFORM_DIR),)
include src/$(PLATFORM_DIR)/Makefile
endif

LOBJS = $(OBJS:.o=.lo)

include src/main/Makefile
include src/plugin/Makefile
include docs/Makefile
include etc/Makefile


XCFLAGS = -Isrc/include -DSYSCONFDIR=\"$(SYSCONFDIR)\" -DVERSION=\"$(VERSION)\"

.SUFFIXES: .c .o .lo .a .so .dll

# Implicit rules for object generation. Position-independent code is intended
# to be used in plugins, so it's also built thread-safe and with the callback
# driver only.

.c.o:
	@CMD='$(CC) $(CFLAGS) $(XCFLAGS) -o $*.o $<'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $*.o ; fi; \
	eval $$CMD

.c.lo:
	@CMD='$(CC) $(CFLAGS) -fPIC -D_REENTRANT -DENABLE_PLUGIN $(XCFLAGS) -o $*.lo $<'; \
	if [ "$(V)" -gt 0 ]; then echo $$CMD; else echo CC $*.lo ; fi; \
	eval $$CMD


binaries: src/main/xmp $(PLUGINS)

clean:
	@rm -f $(OBJS) $(OBJS:.o=.lo) $(M_OBJS)

install: install-xmp install-etc install-docs $(addprefix install-, $(PLUGINS))
	@echo
	@echo "  Installation complete. To customize, copy $(SYSCONFDIR)/xmp.conf"
	@echo "  and $(SYSCONFDIR)/modules.conf to \$$HOME/.xmp/"
	@echo


depend:
	@echo Building dependencies...
	@$(CC) $(CFLAGS) $(XCFLAGS) -MM -MG $(OBJS:.o=.c) >$@

dist: dist-prepare dist-subdirs

dist-prepare:
	rm -Rf $(DIST) $(DIST).tar.gz
	mkdir -p $(DIST)
	cp -rp $(DFILES) $(DIST)/

dist-subdirs: $(addprefix dist-,$(DDIRS))
	chmod -R u+w $(DIST)/*
	tar cvf - $(DIST) | gzip -9c > $(DIST).tar.gz
	rm -Rf $(DIST)
	ls -l $(DIST).tar.gz

$(OBJS): Makefile.rules

$(LOBJS): Makefile.rules

include depend

