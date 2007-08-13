# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.4 2007-08-13 17:18:17 cmatsuoka Exp $

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
	@[ -d $(DEST_DIR)/etc/xmp ] || mkdir -p $(DEST_DIR)/etc/xmp
	@if [ -f $(DEST_DIR)/etc/xmp/xmp.conf ]; then \
	    mv -f $(DEST_DIR)/etc/xmp/xmp.conf $(DEST_DIR)/etc/xmp/xmp.conf.old; \
	fi
	@if [ -f $(DEST_DIR)/etc/xmp/xmp-modules.conf ]; then \
	    mv -f $(DEST_DIR)/etc/xmp/xmp-modules.conf $(DEST_DIR)/etc/xmp/xmp-modules.conf.old; \
	fi
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DEST_DIR)/etc/xmp/

