# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.7 2007-09-16 03:20:05 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp-modules.conf
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include $(TOPDIR)/Makefile.rules

install::
	@echo Installing xmp.conf and xmp-modules.conf in $(ETC_DIR)...
	@[ -d $(DESTDIR)$(ETC_DIR) ] || mkdir -p $(DESTDIR)$(ETC_DIR)
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DESTDIR)$(ETC_DIR)/

