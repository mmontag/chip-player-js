# Extended Module player lib/Makefile
# $Id: Makefile,v 1.1 2001-06-02 20:25:44 cmatsuoka Exp $

DFILES	= Makefile
CFILES	= $(MYLIB) $(MYSHLIB) $(MYSHLIB).$(VERSION) drivers/*
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
	$(INSTALL) -m644 $(MYSHLIB).$(VERSION) $(LIB_DIR)/xmp
	$(INSTALL) -m644 drivers/*.so $(LIB_DIR)/xmp/drivers
endif

shared: $(MYSHLIB).$(VERSION)

$(MYSHLIB).$(VERSION): $(MYLIB)
	rm -f $(MYSHLIB)* *.o
	$(AR) xv $(MYLIB)
	ld -shared -o $(MYSHLIB).$(VERSION) *.o
	rm -f *.o
	ln -s $(MYSHLIB).$(VERSION) $(MYSHLIB)
	
static_install:
	@echo Installing libraries in $(LIB_DIR)...
	@[ -d $(LIB_DIR) ] || mkdir -p $(LIB_DIR)
	$(INSTALL) -m755 $(MYLIB) $(LIB_DIR)
	$(STRIP) -g $(LIB_DIR)/$(MYLIB)


