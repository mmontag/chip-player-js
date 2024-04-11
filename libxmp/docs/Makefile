
DOCS_DFILES	= Makefile COPYING.LIB CREDITS Changelog formats.txt \
		  fixloop.txt libxmp.rst custom.style $(DOCS_FILES)

DOCS_PATH	= docs

DOCS_FILES	= libxmp.html libxmp.3 libxmp.pdf

DOCS += $(addprefix $(DOCS_PATH)/,$(DOCS_FILES))

all-docs: $(DOCS)

$(DOCS): docs/Makefile

install-docs: $(DOCS)
	@echo "Installing manpages in $(DESTDIR)$(MANDIR)"
	@[ -d $(DESTDIR)$(MANDIR) ] || mkdir -p $(DESTDIR)$(MANDIR)
	@$(INSTALL_DATA) docs/libxmp.3 $(DESTDIR)$(MANDIR)

dist-docs:
	mkdir -p $(DIST)/$(DOCS_PATH)
	cp -RPp $(addprefix $(DOCS_PATH)/,$(DOCS_DFILES)) $(DIST)/$(DOCS_PATH)

docs/libxmp.3: docs/libxmp.man.rst
	rst2man docs/libxmp.man.rst > $@

docs/libxmp.man.rst: docs/libxmp.rst docs/manpage-header.rst docs/Makefile
	cp docs/manpage-header.rst $@
	sed -n '/^Introduction/,$$p' docs/libxmp.rst >> $@

docs/libxmp.html: docs/libxmp.rst
	rst2html docs/libxmp.rst --stylesheet docs/style.css > $@

docs/libxmp.pdf: docs/libxmp.rst docs/custom.style
	rst2pdf docs/libxmp.rst -c --smart-quotes=1 -s docs/custom.style --footer="###Page###" -o $@

