# Extended Module Player etc/Makefile
# $Id: Makefile,v 1.1 2001-06-02 20:25:42 cmatsuoka Exp $

DFILES	= Makefile magic xmp.conf xmp.spec xmp.lsm xmp-modules.conf
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include ../Makefile.rules

install::
	@echo Installing xmp.conf and xmp-modules.conf in $(DEST_DIR)...
	@if [ -f $(DEST_DIR)/etc/xmp.conf ]; then \
	    mv -f $(DEST_DIR)/etc/xmp.conf $(DEST_DIR)/etc/xmp.conf.old; \
	fi
	@if [ -f $(DEST_DIR)/etc/xmp-modules.conf ]; then \
	    mv -f $(DEST_DIR)/etc/xmp-modules.conf $(DEST_DIR)/etc/xmp-modules.conf.old; \
	fi
	$(INSTALL) -m644 xmp.conf xmp-modules.conf $(DEST_DIR)/etc

