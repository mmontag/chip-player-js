
DOCS_DFILES	= Makefile COPYING CREDITS README.awebug \
		  README.unsqsh README.fixloop README.trackers ChangeLog \
		  xmp.1 README.os2 pt_versions.txt st02-ok.sample formats
DOCS_PATH	= docs

install-docs:
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL) -m644 docs/xmp.1 $(DESTDIR)$(MANDIR)

dist-docs:
	mkdir -p $(DIST)/$(DOCS_PATH)
	cp -RPp $(addprefix $(DOCS_PATH)/,$(DOCS_DFILES)) $(DIST)/$(DOCS_PATH)

