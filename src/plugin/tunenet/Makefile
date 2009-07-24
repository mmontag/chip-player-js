
TUNENET_OBJS	= init.o obtain.o release.o announceplayer.o initplayer.o \
		  openplayer.o closeplayer.o donotify.o decodeframeplayer.o \
		  exitplayer.o seekplayer.o testplayer.o
TUNENET_DFILES	= Makefile # $(TUNENET_OBJS:.o=.c)
TUNENET_PATH	= src/plugin/tunenet

dist-tunenet:
	mkdir -p $(DIST)/$(TUNENET_PATH)
	cp -dp $(addprefix $(TUNENET_PATH)/,$(TUNENET_DFILES)) $(DIST)/$(TUNENET_PATH)

TN_OBJS = $(addprefix $(TUNENET_PATH)/,$(TUNENET_OBJS))


tn_xmp: $(TUNENET_PATH)/TN_XMP.tnplug

$(TUNENET_PATH)/TN_XMP.tnplug: $(TN_OBJS)
	$(LD) -mcrt=newlib -nostartfiles -o $@ $(TN_OBJS) $(LIBS)

