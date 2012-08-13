
UNXZ_OBJS	= xz_crc32.o xz_dec_lzma2.o xz_dec_stream.o

UNXZ_DFILES	= Makefile $(UNXZ_OBJS:.o=.c) README xz.h xz_config.h \
		  xz_lzma2.h xz_private.h xz_stream.h

UNXZ_PATH	= src/unxz

OBJS += $(addprefix $(UNXZ_PATH)/,$(UNXZ_OBJS))

default-unxz:
	$(MAKE) -C ..

dist-unxz:
	mkdir -p $(DIST)/$(UNXZ_PATH)
	cp -RPp $(addprefix $(UNXZ_PATH)/,$(UNXZ_DFILES)) $(DIST)/$(UNXZ_PATH)

