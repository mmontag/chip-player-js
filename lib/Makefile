# Extended Module player lib/Makefile
# $Id: Makefile,v 1.6 2002-09-03 10:51:53 dmierzej Exp $

SONAME	= 2.1
DFILES	= Makefile
CFILES	= $(MYLIB) $(MYSHLIB) $(MYSHLIB).$(SONAME) drivers/*
DCFILES	= 
MYLIB	= libxmp.a
MYSHLIB	= $(MYLIB:.a=.so)

all: $(MYLIB) dynamic

include ../Makefile.rules

$(MYLIB):
	$(MAKE) -C ../src

ifdef DYNDRV
dynamic: shared
else
dynamic:
endif

install::
ifdef DYNDRV
	[ -d $(LIB_DIR)/xmp/drivers ] || mkdir -p $(LIB_DIR)/xmp/drivers
	$(INSTALL) -m755 $(MYSHLIB).$(SONAME) $(LIB_DIR)
	$(INSTALL) -m755 drivers/*.so $(LIB_DIR)/xmp/drivers
	ldconfig
endif

shared: $(MYSHLIB).$(SONAME)

$(MYSHLIB).$(SONAME): $(MYLIB)
	rm -f $(MYSHLIB)* *.o
	$(AR) xv $(MYLIB)
	ld -shared -soname=$@ -o $@ *.o
	rm -f *.o
	
static_install:
	@echo Installing libraries in $(LIB_DIR)...
	@[ -d $(LIB_DIR) ] || mkdir -p $(LIB_DIR)
	$(INSTALL) -m644 $(MYLIB) $(LIB_DIR)
	$(STRIP) -g $(LIB_DIR)/$(MYLIB)


