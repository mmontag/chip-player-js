# Extended Module Player src/Makefile
# $Id: Makefile,v 1.13 2007-10-21 12:59:15 cmatsuoka Exp $

TOPDIR	= ../
DFILES	= Makefile
_DIRS	= drivers include loaders prowizard player misc main plugin
DDIRS	= $(_DIRS) os2
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS); do	\
		(cd $$i && $(MAKE) $@)	\
	done

include $(TOPDIR)/Makefile.rules
