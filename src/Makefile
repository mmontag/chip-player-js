# Extended Module Player src/Makefile

DFILES	= Makefile
_DIRS	= drivers include loaders player misc main
DDIRS	= $(_DIRS) os2 $(XMMS_DIR)
CFILES	=
DCFILES	=

all install::
	for i in $(_DIRS) $(PLUGIN); do	\
		(cd $$i; $(MAKE) $@)	\
	done

include ../Makefile.rules

