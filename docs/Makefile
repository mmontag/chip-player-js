# Extended Module Player doc/Makefile
# $Id: Makefile,v 1.2 2005-02-21 02:44:44 cmatsuoka Exp $

DOCS	= xmp.1
HTMLS	=
DFILES	= Makefile COPYING CREDITS README.awebug README.ppunpack \
	  README.unsqsh README.fixloop README.trackers ChangeLog xmp.1 xxmp.1 \
	  README.os2
DDIRS	= formats
CFILES	=
DCFILES	=

all: docs

subdist:: docs html

include ../Makefile.rules

docs: $(DOCS)

html: $(HTMLS)

install:: docs
	@echo Installing manpages in $(MAN_DIR)...
	@[ -d $(MAN_DIR) ] || mkdir -p $(MAN_DIR)
	$(INSTALL) -m644 xmp.1 $(MAN_DIR)
	@if test "$(X_XMP)x" = xxmpx; then \
	    $(INSTALL) -m644 $(X_XMP).1 $(MAN_DIR); \
	fi

