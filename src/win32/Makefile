
WIN32_OBJS	= ptpopen.o debug.o usleep.o
WIN32_DFILES	= Makefile $(WIN32_OBJS:.o=.c) unistd.h config.h ptpopen.h \
		  osdcomm.h sys xmp.dsp in_xmp.dsp xmp.dsp.in in_xmp.dsp.in
WIN32_PATH	= src/win32
WIN32_CLEAN	= in_xmp.dsp xmp.dsp

ifeq ($(PLATFORM_DIR),win32)
OBJS += $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))
CLEAN += $(addprefix $(WIN32_PATH)/,$(WIN32_CLEAN))
endif

dist-win32: dsps
	mkdir -p $(DIST)/$(WIN32_PATH)
	cp -drp $(addprefix $(WIN32_PATH)/,$(WIN32_DFILES)) $(DIST)/$(WIN32_PATH)

# Always generate project files with windows drivers
W32DRV_OBJS	= $(addprefix $(DRIVERS_PATH)/,$(DRIVERS_OBJS) win32.o)
W32DRV_LOBJS	= $(addprefix $(DRIVERS_PATH)/,$(DRIVERS_LOBJS))

dsps: src/win32/xmp.dsp src/win32/in_xmp.dsp

src/win32/xmp.dsp: Makefile Makefile.rules src/*/Makefile
	@echo Generate $@
	@rm -f $@.tmp
	@for i in $(OBJS:.o=.c) $(W32DRV_OBJS:.o=.c)  $(M_OBJS:.o=.c); do \
	    echo -e "# Begin Source File\n" >> $@.tmp; \
	    echo $$i | sed 's!^src!SOURCE=..!;s!/!\\!g' >> $@.tmp; \
	    echo "# End Source File" >> $@.tmp; \
	done
	@sed -e 's/@VERSION@/$(VERSION)/g;/@SOURCES@/{r $@.tmp' -e 'd' -e '}' < $@.in > $@
	@rm -f $@.tmp

src/win32/in_xmp.dsp: Makefile Makefile.rules src/*/Makefile
	@echo Generate $@
	@rm -f $@.tmp
	@for i in $(LOBJS:.lo=.c) $(W32DRV_LOBJS:.lo=.c) ; do \
	    echo -e "# Begin Source File\n" >> $@.tmp; \
	    echo $$i | sed 's!^src!SOURCE=..!;s!/!\\!g' >> $@.tmp; \
	    echo "# End Source File" >> $@.tmp; \
	done
	@sed -e 's/@VERSION@/$(VERSION)/g;/@SOURCES@/{r $@.tmp' -e 'd' -e '}' < $@.in > $@
	@rm -f $@.tmp

