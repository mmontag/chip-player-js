
LOADERS	= xm_load.o mod_load.o s3m_load.o stm_load.o 669_load.o far_load.o \
	  mtm_load.o ptm_load.o okt_load.o amd_load.o rad_load.o \
	  ult_load.o mdl_load.o it_load.o stx_load.o pt3_load.o sfx_load.o \
	  flt_load.o st_load.o emod_load.o imf_load.o digi_load.o fnk_load.o \
	  ice_load.o hsc_load.o liq_load.o ims_load.o masi_load.o amf_load.o \
	  psm_load.o stim_load.o mmd_common.o mmd1_load.o mmd3_load.o \
	  rtm_load.o dt_load.o no_load.o arch_load.o sym_load.o med2_load.o \
	  med3_load.o med4_load.o dbm_load.o umx_load.o gdm_load.o pw_load.o \
	  gal5_load.o gal4_load.o mfp_load.o asylum_load.o hmn_load.o \
	  mgt_load.o chip_load.o abk_load.o

LOADERS_OBJS	= common.o iff.o itsex.o asif.o voltable.o sample.o $(LOADERS)
LOADERS_DFILES	= Makefile $(LOADERS_OBJS:.o=.c) \
		  asif.h iff.h it.h loader.h med.h mod.h s3m.h xm.h
LOADERS_PATH	= src/loaders

OBJS += $(addprefix $(LOADERS_PATH)/,$(LOADERS_OBJS))

default:

dist-loaders::
	mkdir -p $(DIST)/$(LOADERS_PATH)
	cp -RPp $(addprefix $(LOADERS_PATH)/,$(LOADERS_DFILES)) $(DIST)/$(LOADERS_PATH)
