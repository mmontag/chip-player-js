# Extended Module player lib/Makefile
# $Id: Makefile,v 1.9 2007-09-15 15:03:38 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile
CFILES	= $(MYLIB)
DCFILES	= 
MYLIB	= libxmp.a
MYLLIB	= libxmp.la

all: $(MYLIB) $(MYLLIB)

include $(TOPDIR)/Makefile.rules

$(MYLIB) $(MYLLIB):
	$(MAKE) -C $(TOPDIR)/src

install::

