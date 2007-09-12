# Extended Module Player src/Makefile
# $Id: Makefile,v 1.12 2007-09-12 14:36:45 cmatsuoka Exp $

TOPDIR	= ../
DFILES	= Makefile
_DIRS	= drivers include loaders prowizard player misc main
DDIRS	= $(_DIRS) os2 bmp
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS) $(PLUGDIR); do	\
		(cd $$i && $(MAKE) $@)	\
	done

include $(TOPDIR)/Makefile.rules
