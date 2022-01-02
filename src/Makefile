
SRC_OBJS	= virtual.o format.o period.o player.o read_event.o dataio.o \
		  misc.o mkstemp.o md5.o lfo.o scan.o control.o far_extras.o \
		  med_extras.o filter.o effects.o mixer.o mix_all.o \
		  load_helpers.o load.o hio.o hmn_extras.o extras.o smix.o \
		  filetype.o memio.o tempfile.o mix_paula.o win32.o

SRC_DFILES	= Makefile $(SRC_OBJS:.o=.c) common.h effects.h \
		  format.h lfo.h list.h mixer.h period.h player.h virtual.h \
		  md5.h precomp_lut.h tempfile.h med_extras.h hio.h \
		  hmn_extras.h extras.h callbackio.h memio.h mdataio.h \
		  far_extras.h paula.h precomp_blep.h

SRC_PATH	= src

OBJS += $(addprefix $(SRC_PATH)/,$(SRC_OBJS))

default-src::
	$(MAKE) -C ..

dist-src::
	mkdir -p $(DIST)/$(SRC_PATH)
	cp -RPp $(addprefix $(SRC_PATH)/,$(SRC_DFILES)) $(DIST)/$(SRC_PATH)

