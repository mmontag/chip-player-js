# Extended Module Player src/loaders/Makefile
# $Id: Makefile,v 1.41 2007-11-16 00:57:29 cmatsuoka Exp $

TOPDIR	= ../..
XCFLAGS	= -I../include -Iinclude
LOADERS	= xm_load.o mod_load.o s3m_load.o stm_load.o 669_load.o far_load.o \
	  mtm_load.o ptm_load.o okt_load.o alm_load.o amd_load.o rad_load.o \
	  ult_load.o mdl_load.o it_load.o stx_load.o pt3_load.o sfx_load.o \
	  flt_load.o st_load.o emod_load.o imf_load.o digi_load.o fnk_load.o \
	  ice_load.o hsc_load.o liq_load.o ims_load.o psm_load.o amf_load.o \
	  svb_load.o stim_load.o mmd1_load.o mmd3_load.o coco_load.o \
	  rtm_load.o dmf_load.o tcb_load.o dt_load.o gtk_load.o no_load.o \
	  arch_load.o sym_load.o dtt_load.o mgt_load.o med3_load.o med4_load.o \
	  ssmt_load.o dbm_load.o stim_load.o umx_load.o
OBJS	= common.o iff.o itsex.o asif.o $(LOADERS)
DFILES	= Makefile $(OBJS:.o=.c) ftm_load.c dbm_load.c med2_load.c
DDIRS	= include
LIB	= $(TOPDIR)/lib/libxmp.a
CFILES	=
DCFILES	= 

all: $(LIB)

include $(TOPDIR)/Makefile.rules

include depend
