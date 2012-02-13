
DOCS_DFILES	= Makefile COPYING.LIB CREDITS \
		  README.unsqsh README.fixloop README.trackers ChangeLog \
		  adlib_sb.txt ay-3-8912.txt formats
DOCS_PATH	= docs

install-docs:
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL_DATA) docs/xmp.1 $(DESTDIR)$(MANDIR)

dist-docs:
	mkdir -p $(DIST)/$(DOCS_PATH)
	cp -RPp $(addprefix $(DOCS_PATH)/,$(DOCS_DFILES)) $(DIST)/$(DOCS_PATH)

