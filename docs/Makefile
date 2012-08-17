
DOCS_DFILES	= Makefile COPYING.LIB ChangeLog
DOCS_PATH	= docs

install-docs:
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL_DATA) docs/xmp.1 $(DESTDIR)$(MANDIR)

dist-docs:
	mkdir -p $(DIST)/$(DOCS_PATH)
	cp -RPp $(addprefix $(DOCS_PATH)/,$(DOCS_DFILES)) $(DIST)/$(DOCS_PATH)

