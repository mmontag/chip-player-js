
WIN32_OBJS	= ptpopen.o debug.o usleep.o
WIN32_DFILES	= Makefile $(WIN32_OBJS:.o=.c) unistd.h config.h ptpopen.h \
		  in_xmp.dsp libxmp.dsp libxmp.dsw osdcomm.h xmp.dsp sys
WIN32_PATH	= src/win32

OBJS += $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))

dist-win32:
	mkdir -p $(DIST)/$(WIN32_PATH)
	cp -dp $(addprefix $(WIN32_PATH)/,$(WIN32_DFILES)) $(DIST)/$(WIN32_PATH)

