# Extended Module Player toplevel Makefile
# $Id: Makefile,v 1.5 2001-11-30 11:03:21 cmatsuoka Exp $

# DIST		distribution package name
# DFILES	standard distribution files 
# DDIRS		standard distribution directories

XCFLAGS	= -Iloaders/include
TEST_XM	=
DIST	= xmp-$(VERSION)
RPMSPEC	= etc/xmp.spec
RPMRC	= etc/rpmrc
MODULES	=
DFILES	= README INSTALL configure configure.in Makefile Makefile.rules.in \
	  $(MODULES)
DDIRS	= lib docs etc src scripts debian modules
CFILES	=
DCFILES	= Makefile.rules.old config.log config.status config.cache

all: xmp

xmp:
	cd src; $(MAKE)

include Makefile.rules

distclean::
	rm -f Makefile.rules

configure: configure.in
	autoconf

check: test

dust:
	@echo "<cough, cough>"

test: xmp
	time -v player/xmp -vvv $(MODULES)

install-plugin:
	$(MAKE) -C src/xmms install

install::
	@echo
	@echo "  Installation complete. To customize, copy /etc/xmp.conf to"
	@echo "  \$$HOME/.xmp/xmp.conf and /etc/xmp-modules.conf to \$$HOME/.xmp/modules.conf"
	@echo

uninstall:
	rm -f $(BIN_DIR)/xmp
	rm -f $(BIN_DIR)/xxmp
	rm -f $(MAN_DIR)/xmp.1
	rm -f $(MAN_DIR)/xxmp.1
	rm -f /etc/xmp.conf /etc/xmp-modules.conf


# Extra targets:
# 'dist' prepares a distribution package
# 'dist-dfsg' prepares a DFSG-compliant distribution package
# 'mark' marks the last RCS revision with the package version number
# 'whatsout' lists the locked files
# 'diff' creates a diff file
# 'rpm' generates a RPM package

dist: docs Makefile dist-dfsg dist-nonfree
	$(MAKE) diff OLDVER=$(VERSION) VERSION=$(VERSION)-nonfree
	mv $(DIST)_$(VERSION)-nonfree.diff.gz $(DIST)-nonfree.patch.gz
	rm $(DIST)-nonfree.tar.gz

dist-pre:
	if [ -x /usr/bin/dh_clean ]; then dh_clean; fi
	rm -Rf $(DIST) $(DIST).tar.gz
	mkdir $(DIST)
	$(MAKE) DISTDIR=$(DIST) subdist
	cat Makefile.rules.in | \
	sed "s/$(DATE)/`date`/" > $(DIST)/Makefile.rules.in
	mv -f Makefile.rules.in Makefile.rules.old
	cp $(DIST)/Makefile.rules.in .
	chmod -R u+w $(DIST)/*

dist-post:
	tar cvf - $(DIST) | gzip -9c > $(DIST).tar.gz
	rm -Rf $(DIST)
	./config.status
	touch -r Makefile.rules.old Makefile.rules.in Makefile.rules
	sync
	ls -l $(DIST).tar.gz

dist-nonfree:
	$(MAKE) dist-pre dist-post DIST=$(DIST)-nonfree

dist-dfsg:
	$(MAKE) dist-pre
	find $(DIST)/src -name "*-dfsg" -exec $(MAKE) dist-wipenonfree FILE={} \;
	(cd $(DIST); a=`grep -n "^59 Temple Place" README`; \
	 mv README README.old; head -$${a%:*} README.old > README; \
	 echo >> README; rm README.old)
	$(MAKE) dist-post

dist-wipenonfree:
	mv $(FILE) `echo $(FILE)|sed 's/-dfsg$$//'`

bz2: dist
	@zcat $(DIST).tar.gz | bzip2 > $(DIST).tar.bz2
	@ls -l $(DIST).tar.bz2
	@sync

bindist:
	$(MAKE) _bindist1 CONFIGURE="./configure"

binpkg: bindist $(PORTS)

_bindist1:
	rm -Rf $(DIST)
	gzip -dc $(DIST).tar.gz | tar xvf -
	cd $(DIST); $(CONFIGURE); $(MAKE) _bindist2
	rm -Rf $(DIST)
	sync

_bindist2: xmp
	strip src/main/xmp src/main/xxmp
	cat src/main/xmp | gzip -9c > ../xmp-$(VERSION)_$(PLATFORM).gz
	cat src/main/xxmp | gzip -9c > ../xxmp-$(VERSION)_$(PLATFORM).gz

mark:
	ID=`echo $(VERSION) | sed 's/\./_/g'`; \
	cvs tag release_$$ID

chkoldver:
	@if [ "$(OLDVER)" = "" ]; then \
	    echo "parameter missing: OLDVER=<old_version>"; \
	    false; \
	fi

diff: chkoldver
	@if [ "$(OLDVER)" != "none" ]; then \
	    echo "Creating diff from $(OLDVER) to $(VERSION)"; \
	    rm -Rf xmp-$(OLDVER) xmp-$(VERSION); \
	    tar xzf xmp-$(OLDVER).tar.gz; \
	    tar xzf xmp-$(VERSION).tar.gz; \
	    diff -rud --new-file xmp-$(OLDVER) xmp-$(VERSION) | gzip -9c > \
		xmp-$(OLDVER)_$(VERSION).diff.gz; \
	    rm -Rf xmp-$(OLDVER) xmp-$(VERSION); \
	    sync; \
	fi

rpm:
	rpm -ba $(RPMSPEC)

deb:
	fakeroot debian/rules binary

graph: delgraph callgraph.ps

delgraph:
	rm -f callgraph.ps

callgraph.ps:
	cflow -gAP -Isrc/include src/{player,misc}/*c 2>/dev/null | \
		scripts/cflow2dot.pl > callgraph.dot
	dot -Tps -o$@ callgraph.dot

Makefile.rules: Makefile.rules.in
	if [ -f config.status ]; then \
	    ./config.status; \
	else \
	    [ -f configure ] || autoconf; \
	    ./configure; \
	fi

$(OBJS): Makefile

