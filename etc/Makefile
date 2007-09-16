# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.8 2007-09-16 13:12:41 cmatsuoka Exp $

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

