
PLAYER_OBJS	= convert.o driver.o formats.o misc.o period.o player.o \
		  readrc.o scan.o ulaw.o cruncher.o med_synth.o filter.o \
		  fmopl.o synth.o mix_all.o effects.o mixer.o ym2149.o \
		  adlib.o spectrum.o
PLAYER_DFILES	= Makefile $(PLAYER_OBJS:.o=.c) effects.c player.h fmopl.h \
		  smix.h ym2149.h
PLAYER_PATH	= src/player

OBJS += $(addprefix $(PLAYER_PATH)/,$(PLAYER_OBJS))

dist-player:
	mkdir -p $(DIST)/$(PLAYER_PATH)
	cp -RPp $(addprefix $(PLAYER_PATH)/,$(PLAYER_DFILES)) $(DIST)/$(PLAYER_PATH)

