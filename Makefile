# Extended Module Player toplevel Makefile
# $Id: Makefile,v 1.22 2007-09-16 03:20:05 cmatsuoka Exp $

# DIST		distribution package name
# DFILES	standard distribution files 
# DDIRS		standard distribution directories

TOPDIR	= .
XCFLAGS	= -Iloaders/include
TEST_XM	=
DIST	= xmp-$(VERSION)
MODULES	= inspiration.s3m
DFILES	= README configure configure.in Makefile Makefile.rules.in \
	  $(MODULES)
DDIRS	= lib docs etc src scripts
CFILES	=
DCFILES	= Makefile.rules.old config.log config.status config.cache

all: xmp

xmp:
	cd src && $(MAKE)

include Makefile.rules

distclean::
	rm -f Makefile.rules

configure: configure.in
	autoconf

install-plugin:
	$(MAKE) -C src/bmp install

install::
	@echo
	@echo "  Installation complete. To customize, copy $(ETC_DIR)/xmp.conf to"
	@echo "  \$$HOME/.xmp/xmp.conf and $(ETC_DIR)/xmp-modules.conf to \$$HOME/.xmp/modules.conf"
	@echo

uninstall:
	rm -f $(BIN_DIR)/xmp
	rm -f $(BIN_DIR)/xxmp
	rm -f $(MAN_DIR)/xmp.1
	rm -f $(MAN_DIR)/xxmp.1
	rm -f /etc/xmp/xmp.conf /etc/xmp/xmp-modules.conf


# Extra targets:
# 'dist' prepares a distribution package
# 'mark' marks the last CVS revision with the package version number
# 'rpm' generates a RPM package

dist:
	rm -Rf $(DIST) $(DIST).tar.gz
	mkdir $(DIST)
	$(MAKE) DISTDIR=$(DIST) subdist
	cat Makefile.rules.in | \
	sed "s/$(DATE)/`date`/" > $(DIST)/Makefile.rules.in
	mv -f Makefile.rules.in Makefile.rules.old
	cp $(DIST)/Makefile.rules.in .
	chmod -R u+w $(DIST)/*
	tar cvf - $(DIST) | gzip -9c > $(DIST).tar.gz
	rm -Rf $(DIST)
	./config.status
	touch -r Makefile.rules.old Makefile.rules.in Makefile.rules
	sync
	ls -l $(DIST).tar.gz

mark:
	cvs tag r`echo $(VERSION) | tr .- _`

Makefile.rules: Makefile.rules.in
	@if [ -f config.status ]; then \
	    ./config.status; \
	else \
	    [ -f configure ] || autoconf; \
	    ./configure; \
	fi

$(OBJS): Makefile

