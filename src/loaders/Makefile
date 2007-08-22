# Extended Module Player src/loaders/Makefile
# $Id: Makefile,v 1.21 2007-08-22 13:26:26 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -Iinclude
LOADERS	= xm_load.o mod_load.o s3m_load.o stm_load.o 669_load.o far_load.o \
	  mtm_load.o ptm_load.o okt_load.o alm_load.o amd_load.o rad_load.o \
	  ult_load.o mdl_load.o it_load.o stx_load.o pt3_load.o sfx_load.o \
	  flt_load.o st_load.o emod_load.o imf_load.o digi_load.o fnk_load.o \
	  ice_load.o hsc_load.o liq_load.o ims_load.o psm_load.o amf_load.o \
	  xann_load.o svb_load.o stim_load.o mmd1_load.o mmd3_load.o \
	  rtm_load.o dmf_load.o tcb_load.o dt_load.o gtk_load.o no_load.o \
	  arch_load.o
LATER	= pru1_load.o pru2_load.o kris_load.o ac1d_load.o fcm_load.o \
	  unic_load.o pp10_load.o pm10_load.o np_load.o \
	  pm20_load.o pm01_load.o wn_load.o pm_load.o \
	  di_load.o mp_load.o ksm_load.o zen_load.o crb_load.o p60a_load.o
OBJS	= common.o iff.o itsex.o $(LOADERS)
DFILES	= Makefile $(OBJS:.o=.c) ftm_load.c tp_load.c dbm_load.c med2_load.c \
	  med4_load.c med_load.c $(LATER:.o=.c)
DDIRS	= include
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	=
DCFILES	= 

all: $(LIB)

test:
	@echo $(LOBJS)

include $(TOPDIR)/Makefile.rules

shared: $(LOBJS)

include depend
