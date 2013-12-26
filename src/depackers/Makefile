
DEPACKERS_OBJS	= ppdepack.o unsqsh.o mmcmp.o readrle.o readlzw.o \
		  unarc.o arcfs.o xfd.o inflate.o muse.o unlzx.o s404_dec.o \
		  unzip.o gunzip.o uncompress.o unxz.o bunzip2.o unlha.o \
		  xz_dec_lzma2.o xz_dec_stream.o oxm.o vorbis.o crc32.o

DEPACKERS_DFILES = Makefile $(DEPACKERS_OBJS:.o=.c) readhuff.h readlzw.h \
		  readrle.h inflate.h xz_lzma2.h README.unxz xz.h \
		  xz_private.h xz_stream.h xz_config.h vorbis.h crc32.h

DEPACKERS_PATH	= src/depackers

OBJS += $(addprefix $(DEPACKERS_PATH)/,$(DEPACKERS_OBJS))

default-depackers::
	$(MAKE) -C ..

dist-depackers::
	mkdir -p $(DIST)/$(DEPACKERS_PATH)
	cp -RPp $(addprefix $(DEPACKERS_PATH)/,$(DEPACKERS_DFILES)) $(DIST)/$(DEPACKERS_PATH)

