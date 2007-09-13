# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.6 2007-09-13 16:44:12 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp-modules.conf
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include $(TOPDIR)/Makefile.rules

install::
	@echo Installing xmp.conf and xmp-modules.conf in /etc/xmp...
	@[ -d $(DESTDIR)/etc/xmp ] || mkdir -p $(DESTDIR)/etc/xmp
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DESTDIR)/etc/xmp/

