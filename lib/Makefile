# Extended Module player lib/Makefile
# $Id: Makefile,v 1.11 2007-09-15 15:12:42 cmatsuoka Exp $

TOPDIR	= ..
DFILES	= Makefile
CFILES	= $(MYLIB) $(MYLIB:.a=-pic.a)
DCFILES	= 
MYLIB	= libxmp.a

all: $(MYLIB)

include $(TOPDIR)/Makefile.rules

$(MYLIB):
	$(MAKE) -C $(TOPDIR)/src

install::

