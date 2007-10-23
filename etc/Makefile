# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.11 2007-10-23 11:28:30 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp-modules.conf xmp.spec
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

