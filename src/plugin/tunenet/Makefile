
TUNENET_OBJS	= Init.o Obtain.o Release.o AnnouncePlayer.o InitPlayer.o \
		  OpenPlayer.o ClosePlayer.o DoNotify.o DecodeFramePlayer.o \
		  ExitPlayer.o SeekPlayer.o TestPlayer.o
TUNENET_DFILES	= Makefile $(TUNENET_OBJS:.o=.c) include Vectors.cpp
TUNENET_PATH	= src/plugin/tunenet

tunenet: tn_xmp

dist-tunenet: $(TUNENET_PATH)/include/TN_XMP.tnplug_rev.h
	mkdir -p $(DIST)/$(TUNENET_PATH)
	cp -RPp $(addprefix $(TUNENET_PATH)/,$(TUNENET_DFILES)) $(DIST)/$(TUNENET_PATH)

TN_OBJS = $(addprefix $(TUNENET_PATH)/,$(TUNENET_OBJS)) $(LOBJS) $(D_LOBJS)


tn_xmp: $(TUNENET_PATH)/XMP.tnplug

$(TUNENET_PATH)/XMP.tnplug: $(TN_OBJS)
	$(LD) -mcrt=newlib -nostartfiles -o $@ $(TN_OBJS) $(LIBS)

$(TUNENET_PATH)/include/TN_XMP.tnplug_rev.h: Makefile
	ver=`echo $(VERSION)|sed 's/\([0-9]*\)\.[0-9]*\.[0-9]*.*/\1/'`; \
	rev=`echo $(VERSION)|sed 's/[0-9]*\.\([0-9]*\)\.[0-9]*.*/\1/'`; \
	sub=`echo $(VERSION)|sed 's/[0-9]*\.[0-9]*\.\([0-9]*\).*/\1/'`; \
	sed "s/@VER@/$$ver/g;s/@REV@/$$rev/g;s/@SUB@/$$sub/g" < $@.in > $@
