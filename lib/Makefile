# Extended Module player lib/Makefile
# $Id: Makefile,v 1.10 2007-09-15 15:07:30 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile
CFILES	= $(MYLIB) $(MYLIB:.a=-pic.a)
DCFILES	= 
MYLIB	= libxmp.a

all: $(MYLIB)

include $(TOPDIR)/Makefile.rules

$(MYLIB) $(MYLLIB):
	$(MAKE) -C $(TOPDIR)/src

install::

