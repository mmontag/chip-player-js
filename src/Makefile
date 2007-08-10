# Extended Module Player src/Makefile
# $Id: Makefile,v 1.11 2007-08-10 20:02:16 cmatsuoka Exp $

TOPDIR	= ../
DFILES	= Makefile
_DIRS	= drivers include loaders prowizard player misc main
DDIRS	= $(_DIRS) os2 bmp
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS) $(PLUGIN); do	\
		(cd $$i && $(MAKE) $@)	\
	done

include $(TOPDIR)/Makefile.rules
