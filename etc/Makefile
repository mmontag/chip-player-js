# Extended Module Player etc/Makefile

TOPDIR	= ..
DFILES	= Makefile xmp.conf xmp.spec
DDIRS	=
CFILES	=
DCFILES	=

all:

subdist:: all

include $(TOPDIR)/Makefile.rules

install::
	@echo Installing xmp.conf in $(SYSCONFDIR)...
	@[ -d $(DESTDIR)$(SYSCONFDIR) ] || mkdir -p $(DESTDIR)$(SYSCONFDIR)
	$(INSTALL) -m644 xmp.conf $(DESTDIR)$(SYSCONFDIR)/

