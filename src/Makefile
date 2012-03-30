
SRC_OBJS	= convert.o virtual.o format.o misc.o period.o player.o \
		  read_event.o dataio.o mkstemp.o fnmatch.o \
		  stepper.o lfo.o envelope.o control.o scan.o med_synth.o \
		  filter.o fmopl.o synth_null.o mix_all.o effects.o mixer.o \
		  ym2149.o adlib.o spectrum.o invloop.o load_helpers.o \
		  load.o ppdepack.o unsqsh.o mmcmp.o readrle.o readhuff.o \
		  readlzw.o unarc.o arcfs.o oxm.o xfd.o inflate.o muse.o \
		  unlzx.o s404_dec.o unzip.o gunzip.o uncompress.o

SRC_DFILES	= Makefile $(SRC_OBJS:.o=.c) common.h convert.h effects.h \
		  envelope.h fmopl.h format.h lfo.h list.h mixer.h \
		  period.h player.h readhuff.h readlzw.h readrle.h spectrum.h \
		  stepper.h synth.h virtual.h ym2149.h inflate.h

SRC_PATH	= src

OBJS += $(addprefix $(SRC_PATH)/,$(SRC_OBJS))

default-src:
	$(MAKE) -C ..

dist-src:
	mkdir -p $(DIST)/$(SRC_PATH)
	cp -RPp $(addprefix $(SRC_PATH)/,$(SRC_DFILES)) $(DIST)/$(SRC_PATH)

