
DOCS_DFILES	= Makefile COPYING.LIB CREDITS ChangeLog tracker_notes.txt \
		  $(DOCS_FILES)

DOCS_PATH	= docs

DOCS_FILES	= libxmp.rst libxmp.html libxmp.3 libxmp.pdf

DOCS += $(addprefix $(DOCS_PATH)/,$(DOCS_FILES))

all-docs: $(DOCS)

install-docs: $(DOCS)
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL_DATA) docs/libxmp.3 $(DESTDIR)$(MANDIR)

dist-docs:
	mkdir -p $(DIST)/$(DOCS_PATH)
	cp -RPp $(addprefix $(DOCS_PATH)/,$(DOCS_DFILES)) $(DIST)/$(DOCS_PATH)

docs/libxmp.3: docs/libxmp.rst
	rst2man $+ > $@
	sed -i 's/^\.TH/.TH "LIBXMP" "3" "Version 4.0" "Feb 2013" "Extended Module Player"/' $@

docs/libxmp.html: docs/libxmp.rst
	rst2html $+ > $@

docs/libxmp.pdf: docs/libxmp.rst
	rst2pdf $+

