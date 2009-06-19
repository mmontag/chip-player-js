# Extended Module Player src/Makefile

TOPDIR	= ../
DFILES	= Makefile
_DIRS	= drivers include loaders player misc main plugin
DDIRS	= $(_DIRS) os2 win32
CFILES	=
DCFILES	=

all install::
	@for i in $(_DIRS) $(PLATFORM_DIR); do	\
		(cd $$i && $(MAKE) $@)	\
	done

include $(TOPDIR)/Makefile.rules
