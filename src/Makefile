# Extended Module Player src/Makefile

DFILES	= Makefile
_DIRS	= drivers include loaders player misc main
DDIRS	= $(_DIRS) os2 $(XMMS)
CFILES	=
DCFILES	=
LINKS   = xpanel.c fft4g.c font1.c font2.c

all install::
	-ln -sf ../player/driver.c $(PLUGIN)/
	-for i in $(LINKS); do   \
	ln -sf ../main/$$i $(PLUGIN)/ ; \
	done
	for i in $(_DIRS) $(PLUGIN); do	\
		(cd $$i; $(MAKE) $@)	\
	done

include ../Makefile.rules
