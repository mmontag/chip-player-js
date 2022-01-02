
SRC_OBJS	= virtual.o format.o period.o player.o read_event.o \
		  misc.o dataio.o lfo.o scan.o control.o filter.o \
		  effects.o mixer.o mix_all.o load_helpers.o load.o \
		  filetype.o hio.o smix.o memio.o win32.o

SRC_DFILES	= Makefile $(SRC_OBJS:.o=.c) md5.c md5.h common.h effects.h \
		  format.h lfo.h list.h mixer.h period.h player.h virtual.h \
		  precomp_lut.h hio.h callbackio.h memio.h mdataio.h tempfile.h

SRC_PATH	= src

OBJS += $(addprefix $(SRC_PATH)/,$(SRC_OBJS))

default-src::
	$(MAKE) -C ..

dist-src::
	mkdir -p $(DIST)/$(SRC_PATH)
	cp -RPp $(addprefix $(SRC_PATH)/,$(SRC_DFILES)) $(DIST)/$(SRC_PATH)

