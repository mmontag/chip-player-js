# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.12 2009-06-13 13:28:29 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp.spec
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include $(TOPDIR)/Makefile.rules

install::
	@echo Installing xmp.conf in $(SYSCONFDIR)...
	@[ -d $(DESTDIR)$(SYSCONFDIR) ] || mkdir -p $(DESTDIR)$(SYSCONFDIR)
	$(INSTALL) -m644 xmp.conf $(DESTDIR)$(SYSCONFDIR)/

