
PROWIZ_OBJS	= prowiz.o ptktable.o tuning.o ac1d.o di.o eureka.o \
		  fc-m.o fuchs.o fuzzac.o gmc.o heatseek.o ksm.o \
		  mp.o np1.o np2.o np3.o p61a.o pm10c.o pm18a.o \
		  pha.o prun1.o prun2.o tdd.o unic.o unic2.o wn.o zen.o \
		  tp1.o tp3.o p40.o xann.o theplayer.o pp10.o pp21.o \
		  starpack.o titanics.o skyt.o novotrade.o hrt.o noiserun.o
PROWIZ_OBJS2	= pm.o pm01.o pm20.o pm40.o pp30.o
PROWIZ_DFILES	= Makefile $(PROWIZ_OBJS:.o=.c) $(PROWIZ_OBJS2:.o=.c) prowiz.h
PROWIZ_PATH	= src/loaders/prowizard

OBJS += $(addprefix $(PROWIZ_PATH)/, $(PROWIZ_OBJS))

dist-prowiz::
	mkdir -p $(DIST)/$(PROWIZ_PATH)
	cp -RPp $(addprefix $(PROWIZ_PATH)/,$(PROWIZ_DFILES)) $(DIST)/$(PROWIZ_PATH)


