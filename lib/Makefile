# Extended Module player lib/Makefile

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

