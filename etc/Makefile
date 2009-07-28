
ETC_DFILES	= Makefile xmp.conf modules.conf xmp.spec
ETC_PATH	= etc

install-etc:
	@[ -d $(DESTDIR)$(SYSCONFDIR) ] || mkdir -p $(DESTDIR)$(SYSCONFDIR)
	@echo "Installing xmp.conf in $(SYSCONFDIR)"
	@$(INSTALL) -m644 $(ETC_PATH)/xmp.conf $(DESTDIR)$(SYSCONFDIR)/
	@echo "Installing modules.conf in $(SYSCONFDIR)"
	@$(INSTALL) -m644 $(ETC_PATH)/modules.conf $(DESTDIR)$(SYSCONFDIR)/

dist-etc:
	mkdir -p $(DIST)/$(ETC_PATH)
	cp -rp $(addprefix $(ETC_PATH)/,$(ETC_DFILES)) $(DIST)/$(ETC_PATH)


