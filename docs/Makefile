# Extended Module Player doc/Makefile
# $Id: Makefile,v 1.8 2007-09-16 03:20:05 cmatsuoka Exp $

TOPDIR	= ..
DOCS	= xmp.1
HTMLS	=
DFILES	= Makefile COPYING CREDITS README.awebug \
	  README.unsqsh README.fixloop README.trackers ChangeLog xmp.1 \
	  README.os2 pt_versions.txt st02-ok.sample
DDIRS	= formats
CFILES	=
DCFILES	=

all: docs

subdist:: docs html

include $(TOPDIR)/Makefile.rules

docs: $(DOCS)

html: $(HTMLS)

install:: docs
	@echo Installing manpages in $(DESTDIR)$(MAN_DIR)...
	@[ -d $(DESTDIR)$(MAN_DIR) ] || mkdir -p $(DESTDIR)$(MAN_DIR)
	$(INSTALL) -m644 xmp.1 $(DESTDIR)$(MAN_DIR)
