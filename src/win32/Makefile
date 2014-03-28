
WIN32_OBJS	= ptpopen.o debug.o
WIN32_DFILES	= Makefile $(WIN32_OBJS:.o=.c) unistd.h ptpopen.h \
		  osdcomm.h sys
WIN32_PATH	= src/win32

ifeq ($(PLATFORM_DIR),win32)
OBJS += $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))
CLEAN += $(addprefix $(WIN32_PATH)/,$(WIN32_CLEAN))
endif

dist-win32:
	mkdir -p $(DIST)/$(WIN32_PATH)
	cp -RPp $(addprefix $(WIN32_PATH)/,$(WIN32_DFILES)) $(DIST)/$(WIN32_PATH)

W_OBJS		= $(addprefix $(WIN32_PATH)/,$(WIN32_OBJS))

