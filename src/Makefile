# Extended Module Player src/Makefile
# $Id: Makefile,v 1.8 2006-02-12 22:47:51 cmatsuoka Exp $

DFILES	= Makefile
_DIRS	= drivers include loaders prowizard player misc main
DDIRS	= $(_DIRS) os2 $(XMMS)
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS) $(PLUGIN); do	\
		(cd $$i; $(MAKE) $@)	\
	done

include ../Makefile.rules
