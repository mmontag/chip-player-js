
DOCS_DFILES	= Makefile COPYING CREDITS README.awebug \
		  README.unsqsh README.fixloop README.trackers ChangeLog \
		  xmp.1 README.os2 pt_versions.txt st02-ok.sample

install-docs:
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL) -m644 docs/xmp.1 $(DESTDIR)$(MANDIR)
