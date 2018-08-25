
INCLUDE_DFILES	= Makefile xmp.h

INCLUDE_PATH	= include/libxmp-lite

install-include:
	$(INSTALL_DATA) xmp.h $(DESTDIR)$(INCLUDEDIR)

dist-include:
	mkdir -p $(DIST)/$(INCLUDE_PATH)
	cp -RPp $(addprefix $(INCLUDE_PATH)/,$(INCLUDE_DFILES)) $(DIST)/$(INCLUDE_PATH)

