# Extended Module Player src/Makefile
# $Id: Makefile,v 1.7 2005-02-23 14:25:49 cmatsuoka Exp $

DFILES	= Makefile
_DIRS	= drivers include loaders player misc main
DDIRS	= $(_DIRS) os2 $(XMMS)
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS) $(PLUGIN); do	\
		(cd $$i; $(MAKE) $@)	\
	done

include ../Makefile.rules
