
WIN32_OBJS	= ptpopen.o debug.o usleep.o
WIN32_DFILES	= Makefile $(WIN32_OBJS:.o=.c) unistd.h ptpopen.h \
		  osdcomm.h sys libxmp.dsp libxmp.dsp.in
WIN32_PATH	= src/win32
WIN32_CLEAN	= in_xmp.dsp xmp.dsp

ifeq ($(PLATFORM_DIR),win32)
OBJS += $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))
CLEAN += $(addprefix $(WIN32_PATH)/,$(WIN32_CLEAN))
endif

dist-win32: dsps
	mkdir -p $(DIST)/$(WIN32_PATH)
	cp -RPp $(addprefix $(WIN32_PATH)/,$(WIN32_DFILES)) $(DIST)/$(WIN32_PATH)

W_OBJS		= $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))

dsps: src/win32/libxmp.dsp

src/win32/libxmp.dsp: Makefile src/Makefile src/loaders/Makefile src/loaders/prowizard/Makefile src/win32/libxmp.dsp.in
	@echo Generate $@
	@rm -f $@.tmp
	@for i in $(OBJS:.o=.c) $(W_OBJS:.o=.c); do \
	    echo "# Begin Source File" >> $@.tmp; \
	    echo >> $@.tmp; \
	    echo $$i | sed 's!^src!SOURCE=..!;s!/!\\!g' >> $@.tmp; \
	    echo "# End Source File" >> $@.tmp; \
	done
	@sed -e 's/@VERSION@/$(VERSION)/g;/@SOURCES@/{r $@.tmp' -e 'd' -e '}' < $@.in > $@
	@rm -f $@.tmp
	@unix2dos $@
