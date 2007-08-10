# Makefile for use under OS/2
#
# Hand build by Kevin Langman
#
# This Makefile was tested with dmake only...
# I don't know if the "+del version.o" will work with GNU Make!
#

TOPDIR	= ../..
VERSION	= 2.0.2
DATE	= Wed Feb 23 09:14:49 BRST 2000


XMP_OS2_BUILDER = Kevin Langman (langman@earthling.net)

DRIVER_OBJ= file.o os2_dart.o
AR=emxomfar
ARFLAGS=cr
CC=gcc
DFILES= Makefile version.c xmp.def xmp2.txt

# -O2                  = Optimize 2
# -ffast-math          = Allows gcc to break some ANSI math rules!
# -fno-strength-reduce = ???
# -Wall                = All Warnings!
# -s                   = Remove all symbol table and relocation information
# -fomit-frame-pointer = Don't keep the frame pointer in a register
# -fpack-struct        = Pack all Structures!

# Use the fist   CFLAGS & LFLAGS to create a OPTIMIZED build!
# Use the second CFLAGS & LFLAGS to create a build that can be debugged via ICSDEBUG.exe 

CFLAGS=-O2 -Zomf -Zmt -DDRIVER_OS2DART -DHAVE_ATTRIBUTE_PACKED -funroll-loops -ffast-math -fno-strength-reduce -Wall -s -fomit-frame-pointer
# CFLAGS=-g -Zomf -Zmt -DDRIVER_OS2DART -DHAVE_ATTRIBUTE_PACKED -Wall

LFLAGS = -Zlinker /PM:VIO
# LFLAGS = -Zlinker /DEBUG -Zlinker /PM:VIO

COMPILE=$(CC) $(CFLAGS) -I../include -I../loaders/include -I..
IMPLIB=emximp

.SUFFIXES:
.SUFFIXES: .o .c

LIBS=-lmmpm2
EXENAME=xmp2.exe
DEFNAME=xmp.def
OBJ=$(DRIVER_OBJ) \
    control.o crc32.o depack.o info.o ipc.o load.o mmcmp.o unsqsh.o \
    common.o iff.o itsex.o 669_load.o ac1d_load.o alm_load.o amd_load.o \
    crb_load.o dbm_load.o digi_load.o di_load.o emod_load.o far_load.o \
    fcm_load.o flt_load.o fnk_load.o imf_load.o it_load.o kris_load.o \
    ksm_load.o liq_load.o mdl_load.o med_load.o mod_load.o mp_load.o \
    mtm_load.o np_load.o okt_load.o pm01_load.o pm10_load.o pm20_load.o \
    pm_load.o pp10_load.o pru1_load.o pru2_load.o pt3_load.o ptm_load.o \
    rad_load.o s3m_load.o sfx_load.o stim_load.o stm_load.o stx_load.o \
    st_load.o tp_load.o ult_load.o unic_load.o wn_load.o xann_load.o \
    xm_load.o zen_load.o ice_load.o font1.o font2.o getopt.o getopt1.o \
    main.o options.o convert.o cruncher.o driver.o formats.o med_synth.o \
    misc.o mix_all.o period.o player.o readrc.o scan.o ulaw.o version.o

default: xmp2.exe

clean:

clean2:
	+del *.o
	+del version.c

install:

# This target will allow the nice-very-nice packaging system to work
# with this subdirectory

subdist::
	cp -dp $(DFILES) $(DISTDIR)


# The '+del version.o' will cause the build Date&Time to be recalculated for each build!

$(EXENAME): $(OBJ) $(DEFNAME)
	$(CC) $(CFLAGS) -o $(EXENAME) $(DEFNAME) $(OBJ) $(LIBS) $(LFLAGS)
	+del version.o
  
version.c:
	@echo char *xmp_version = "$(VERSION)"; \
	char *xmp_date = "$(DATE)";>$@ \
	char *xmp_build= "Compiled by $(XMP_OS2_BUILDER) on " __DATE__ " at " __TIME__;>>$@

version.o:	version.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c version.c	

file.o:	../drivers/file.c \
			../include/xmpi.h ../include/driver.h ../include/mixer.h
	$(COMPILE) -o $@ -c ../drivers/file.c
os2_dart.o:	../drivers/os2_dart.c \
			../include/xmpi.h ../include/driver.h ../include/mixer.h
	$(COMPILE) -o $@ -c ../drivers/os2_dart.c



control.o:	../misc/control.c \
			../include/driver.h ../include/mixer.h ../include/formats.h
	$(COMPILE) -o $@ -c ../misc/control.c
crc32.o:	../misc/crc32.c \
			../include/formats.h
	$(COMPILE) -o $@ -c ../misc/crc32.c
depack.o:	../misc/depack.c \
			../include/xmpi.h 
	$(COMPILE) -o $@ -c ../misc/depack.c
info.o:	../misc/info.c \
			../include/driver.h 
	$(COMPILE) -o $@ -c ../misc/info.c
ipc.o:	../misc/ipc.c \
			../include/xmpi.h 
	$(COMPILE) -o $@ -c ../misc/ipc.c
load.o:	../misc/load.c \
			../include/driver.h ../include/convert.h
	$(COMPILE) -o $@ -c ../misc/load.c
mmcmp.o:	../misc/mmcmp.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../misc/mmcmp.c
unsqsh.o:	../misc/unsqsh.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../misc/unsqsh.c



common.o:	../loaders/common.c \
			../include/xmp.h ../include/period.h ../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/common.c
iff.o:	../loaders/iff.c \
			../loaders/include/iff.h ../include/xmpi.h
	$(COMPILE) -o $@ -c ../loaders/iff.c
itsex.o:	../loaders/itsex.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/itsex.c


669_load.o:	../loaders/669_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/669_load.c
ac1d_load.o:	../loaders/ac1d_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/ac1d_load.c
alm_load.o:	../loaders/alm_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/alm_load.c
amd_load.o:	../loaders/amd_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/amd_load.c
crb_load.o:	../loaders/crb_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/crb_load.c
dbm_load.o:	../loaders/dbm_load.c \
			../loaders/include/load.h ../loaders/include/iff.h
	$(COMPILE) -o $@ -c ../loaders/dbm_load.c
digi_load.o:	../loaders/digi_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/digi_load.c
di_load.o:	../loaders/di_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/di_load.c
emod_load.o:	../loaders/emod_load.c \
			../loaders/include/load.h ../loaders/include/iff.h
	$(COMPILE) -o $@ -c ../loaders/emod_load.c
far_load.o:	../loaders/far_load.c \
			../loaders/include/load.h ../loaders/include/far.h
	$(COMPILE) -o $@ -c ../loaders/far_load.c
fcm_load.o:	../loaders/fcm_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/fcm_load.c
flt_load.o:	../loaders/flt_load.c \
			../loaders/include/load.h ../loaders/include/mod.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/flt_load.c
fnk_load.o:	../loaders/fnk_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/fnk_load.c
ftm_load.o:	../loaders/ftm_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/ftm_load.c
imf_load.o:	../loaders/imf_load.c \
			../loaders/include/load.h ../loaders/include/imf.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/imf_load.c
it_load.o:	../loaders/it_load.c \
			../loaders/include/load.h ../loaders/include/it.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/it_load.c
kris_load.o:	../loaders/kris_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/kris_load.c
ksm_load.o:	../loaders/ksm_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/ksm_load.c
liq_load.o:	../loaders/liq_load.c \
			../loaders/include/load.h ../loaders/include/liq.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/liq_load.c
mdl_load.o:	../loaders/mdl_load.c \
			../loaders/include/load.h ../loaders/include/iff.h ../loaders/include/mdl.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/mdl_load.c
med_load.o:	../loaders/med_load.c \
			../loaders/include/load.h ../loaders/include/med.h
	$(COMPILE) -o $@ -c ../loaders/med_load.c
mod_load.o:	../loaders/mod_load.c \
			../loaders/include/load.h ../loaders/include/mod.h
	$(COMPILE) -o $@ -c ../loaders/mod_load.c
mp_load.o:	../loaders/mp_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/mp_load.c
mtm_load.o:	../loaders/mtm_load.c \
			../loaders/include/load.h ../loaders/include/mtm.h
	$(COMPILE) -o $@ -c ../loaders/mtm_load.c
np_load.o:	../loaders/np_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/np_load.c
okt_load.o:	../loaders/okt_load.c \
			../loaders/include/load.h ../loaders/include/iff.h
	$(COMPILE) -o $@ -c ../loaders/okt_load.c
pm01_load.o:	../loaders/pm01_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/pm01_load.c
pm10_load.o:	../loaders/pm10_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/pm10_load.c
pm20_load.o:	../loaders/pm20_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/pm20_load.c
pm_load.o:	../loaders/pm_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/pm_load.c
pp10_load.o:	../loaders/pp10_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/pp10_load.c
pru1_load.o:	../loaders/pru1_load.c \
			../loaders/include/load.h ../loaders/include/mod.h
	$(COMPILE) -o $@ -c ../loaders/pru1_load.c
pru2_load.o:	../loaders/pru2_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/pru2_load.c
pt3_load.o:	../loaders/pt3_load.c \
			../loaders/include/load.h ../loaders/include/iff.h
	$(COMPILE) -o $@ -c ../loaders/pt3_load.c
ptm_load.o:	../loaders/ptm_load.c \
			../loaders/include/load.h ../loaders/include/ptm.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/ptm_load.c
rad_load.o:	../loaders/rad_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/rad_load.c
s3m_load.o:	../loaders/s3m_load.c \
			../loaders/include/load.h ../loaders/include/s3m.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/s3m_load.c
sfx_load.o:	../loaders/sfx_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/sfx_load.c
stim_load.o:	../loaders/stim_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/stim_load.c
stm_load.o:	../loaders/stm_load.c \
			../loaders/include/load.h ../loaders/include/stm.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/stm_load.c
stx_load.o:	../loaders/stx_load.c \
			../loaders/include/load.h ../loaders/include/stx.h ../loaders/include/s3m.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/stx_load.c
st_load.o:	../loaders/st_load.c \
			../loaders/include/load.h ../loaders/include/mod.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/st_load.c
tp_load.o:	../loaders/tp_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/tp_load.c
ult_load.o:	../loaders/ult_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/ult_load.c
unic_load.o:	../loaders/unic_load.c \
			../loaders/include/load.h ../include/period.h
	$(COMPILE) -o $@ -c ../loaders/unic_load.c
wn_load.o:	../loaders/wn_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/wn_load.c
xann_load.o:	../loaders/xann_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/xann_load.c
xm_load.o:	../loaders/xm_load.c \
			../loaders/include/load.h ../loaders/include/xm.h
	$(COMPILE) -o $@ -c ../loaders/xm_load.c
zen_load.o:	../loaders/zen_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/zen_load.c
ice_load.o:	../loaders/ice_load.c \
			../loaders/include/load.h
	$(COMPILE) -o $@ -c ../loaders/ice_load.c



font1.o:	../main/font1.c \
			../main/xpanel.h
	$(COMPILE) -o $@ -c ../main/font1.c
font2.o:	../main/font2.c \
			../main/xpanel.h
	$(COMPILE) -o $@ -c ../main/font2.c
getopt.o:	../main/getopt.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../main/getopt.c
getopt1.o:	../main/getopt1.c \
			../include/getopt.h
	$(COMPILE) -o $@ -c ../main/getopt1.c
main.o:	../main/main.c \
			../include/xmp.h
	$(COMPILE) -o $@ -c ../main/main.c
options.o:	../main/options.c \
			../include/xmp.h
	$(COMPILE) -o $@ -c ../main/options.c
xpanel.o:	../main/xpanel.c \
			../main/xpanel.h
	$(COMPILE) -o $@ -c ../main/xpanel.c
xstuff.o:	../main/xstuff.c \
			../main/xpanel.h
	$(COMPILE) -o $@ -c ../main/xstuff.c




convert.o:	../player/convert.c \
			../include/xmp.h ../include/convert.h ../include/driver.h
	$(COMPILE) -o $@ -c ../player/convert.c
cruncher.o:	../player/cruncher.c \
			../include/driver.h ../include/convert.h ../include/mixer.h
	$(COMPILE) -o $@ -c ../player/cruncher.c
driver.o:	../player/driver.c \
			../include/driver.h ../include/convert.h ../include/mixer.h ../include/period.h
	$(COMPILE) -o $@ -c ../player/driver.c
formats.o:	../player/formats.c \
			../include/xmpi.h ../include/formats.h
	$(COMPILE) -o $@ -c ../player/formats.c
med_synth.o:	../player/med_synth.c \
			../include/xmpi.h ../player/player.h ../include/driver.h
	$(COMPILE) -o $@ -c ../player/med_synth.c
misc.o:	../player/misc.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../player/misc.c
mixer.o:	../player/mixer.c \
			../include/mixer.h
	$(COMPILE) -o $@ -c ../player/mixer.c
mix_all.o:	../player/mix_all.c \
			../include/xmpi.h ../include/driver.h ../include/mixer.h
	$(COMPILE) -o $@ -c ../player/mix_all.c
period.o:	../player/period.c \
			../include/xmpi.h ../include/period.h
	$(COMPILE) -o $@ -c ../player/period.c
player.o:	../player/player.c \
			../player/effects.c ../include/driver.h ../include/period.h ../include/effects.h ../player/player.h
	$(COMPILE) -o $@ -c ../player/player.c
readrc.o:	../player/readrc.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../player/readrc.c
scan.o:	../player/scan.c \
			../include/xmpi.h ../include/effects.h
	$(COMPILE) -o $@ -c ../player/scan.c
ulaw.o:	../player/ulaw.c \
			../include/xmpi.h
	$(COMPILE) -o $@ -c ../player/ulaw.c
	

