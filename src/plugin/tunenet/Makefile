
TUNENET_OBJS	= Init.o Obtain.o Release.o AnnouncePlayer.o InitPlayer.o \
		  OpenPlayer.o ClosePlayer.o DoNotify.o DecodeFramePlayer.o \
		  ExitPlayer.o SeekPlayer.o TestPlayer.o
TUNENET_DFILES	= Makefile # $(TUNENET_OBJS:.o=.c)
TUNENET_PATH	= src/plugin/tunenet

dist-tunenet:
	mkdir -p $(DIST)/$(TUNENET_PATH)
	cp -dp $(addprefix $(TUNENET_PATH)/,$(TUNENET_DFILES)) $(DIST)/$(TUNENET_PATH)

TN_OBJS = $(addprefix $(TUNENET_PATH)/,$(TUNENET_OBJS))


tn_xmp: $(TUNENET_PATH)/TN_XMP.tnplug

$(TUNENET_PATH)/TN_XMP.tnplug: $(TN_OBJS)
	$(LD) -mcrt=newlib -nostartfiles -o $@ $(TN_OBJS) $(LIBS)

