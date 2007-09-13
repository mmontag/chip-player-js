# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.5 2007-09-13 13:29:47 cmatsuoka Exp $

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
	@if [ -f $(DESTDIR)/etc/xmp/xmp.conf ]; then \
	    mv -f $(DESTDIR)/etc/xmp/xmp.conf $(DESTDIR)/etc/xmp/xmp.conf.old; \
	fi
	@if [ -f $(DEST_DIR)/etc/xmp/xmp-modules.conf ]; then \
	    mv -f $(DESTDIR)/etc/xmp/xmp-modules.conf $(DESTDIR)/etc/xmp/xmp-modules.conf.old; \
	fi
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DESTDIR)/etc/xmp/

