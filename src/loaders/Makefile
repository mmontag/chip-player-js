# Extended Module Player src/loaders/Makefile
# $Id: Makefile,v 1.3 2005-02-09 17:12:55 cmatsuoka Exp $

XCFLAGS	= -I../include -Iinclude
LOADERS	= xm_load.o mod_load.o s3m_load.o stm_load.o 669_load.o far_load.o \
	  mtm_load.o ptm_load.o okt_load.o alm_load.o amd_load.o rad_load.o \
	  ult_load.o pru1_load.o pru2_load.o kris_load.o ac1d_load.o \
	  fcm_load.o unic_load.o pp10_load.o xann_load.o pm10_load.o \
	  mdl_load.o np_load.o it_load.o stx_load.o stim_load.o pm20_load.o \
	  pm01_load.o pt3_load.o sfx_load.o flt_load.o st_load.o \
	  emod_load.o wn_load.o pm_load.o di_load.o mp_load.o imf_load.o \
	  digi_load.o ksm_load.o zen_load.o crb_load.o fnk_load.o ice_load.o \
	  p60a_load.o hsc_load.o liq_load.o ims_load.o
OBJS	= common.o iff.o itsex.o $(LOADERS)
DFILES	= Makefile $(OBJS:.o=.c) ftm_load.c tp_load.c dbm_load.c med2_load.c \
	  med4_load.c med_load.c
DDIRS	= include
LIB	= ../../lib/libxmp.a
CFILES	=
DCFILES	= 

all: $(LIB)

include ../../Makefile.rules

include depend
