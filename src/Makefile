
SRC_OBJS	= virtual.o format.o period.o player.o read_event.o dataio.o \
		  mkstemp.o fnmatch.o md5.o lfo.o envelope.o scan.o control.o \
		  med_extras.o filter.o effects.o mixer.o mix_all.o \
		  load_helpers.o load.o hio.o hmn_extras.o extras.o smix.o \
		  memio.o tempfile.o mix_paula.o

SRC_DFILES	= Makefile $(SRC_OBJS:.o=.c) common.h effects.h envelope.h \
		  format.h lfo.h list.h mixer.h period.h player.h virtual.h \
		  fnmatch.h md5.h precomp_lut.h tempfile.h med_extras.h hio.h \
		  hmn_extras.h extras.h memio.h mdataio.h depacker.h paula.h \
		  precomp_blep.h

SRC_PATH	= src

OBJS += $(addprefix $(SRC_PATH)/,$(SRC_OBJS))

default-src::
	$(MAKE) -C ..

dist-src::
	mkdir -p $(DIST)/$(SRC_PATH)
	cp -RPp $(addprefix $(SRC_PATH)/,$(SRC_DFILES)) $(DIST)/$(SRC_PATH)

