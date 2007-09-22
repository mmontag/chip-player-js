# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.10 2007-09-22 12:49:04 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp-modules.conf
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include $(TOPDIR)/Makefile.rules

install::
	@echo Installing xmp.conf and xmp-modules.conf in $(SYSCONFDIR)...
	@[ -d $(DESTDIR)$(SYSCONFDIR) ] || mkdir -p $(DESTDIR)$(SYSCONFDIR)
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DESTDIR)$(SYSCONFDIR)/

