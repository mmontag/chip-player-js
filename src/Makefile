
SRC_OBJS	= virtual.o format.o misc.o period.o player.o read_event.o \
		  dataio.o mkstemp.o fnmatch.o md5.o lfo.o envelope.o scan.o \
		  control.o med_synth.o filter.o fmopl.o effects.o mixer.o \
		  synth_null.o mix_all.o ym2149.o adlib.o spectrum.o \
		  load_helpers.o load.o oxm.o vorbis.o

SRC_DFILES	= Makefile $(SRC_OBJS:.o=.c) common.h effects.h envelope.h \
		  fmopl.h format.h lfo.h list.h mixer.h period.h player.h \
		  spectrum.h synth.h virtual.h ym2149.h fnmatch.h vorbis.h \
		  md5.h precomp_lut.h med_extras.h

SRC_PATH	= src

OBJS += $(addprefix $(SRC_PATH)/,$(SRC_OBJS))

default-src:
	$(MAKE) -C ..

dist-src:
	mkdir -p $(DIST)/$(SRC_PATH)
	cp -RPp $(addprefix $(SRC_PATH)/,$(SRC_DFILES)) $(DIST)/$(SRC_PATH)

