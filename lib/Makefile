# Extended Module player lib/Makefile
# $Id: Makefile,v 1.4 2001-11-15 09:59:54 cmatsuoka Exp $

SONAME	= 2.0
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
	$(INSTALL) -m644 $(MYSHLIB).$(SONAME) $(LIB_DIR)
	$(INSTALL) -m644 drivers/*.so $(LIB_DIR)/xmp/drivers
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


