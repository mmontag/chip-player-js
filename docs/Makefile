# Extended Module Player doc/Makefile
# $Id: Makefile,v 1.9 2007-09-16 13:12:41 cmatsuoka Exp $

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
	@echo Installing manpages in $(DESTDIR)$(MANDIR)...
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	$(INSTALL) -m644 xmp.1 $(DESTDIR)$(MANDIR)
