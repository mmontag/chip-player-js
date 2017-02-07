#ifndef __SN764INTF_H__
#define __SN764INTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_SN76496_MAME		// enable SN76496 core from MAME
#define EC_SN76496_MAXIM	// enable SN76489 core by Maxim (from in_vgm)


typedef struct sn76496_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT16 noiseTaps;
	UINT8 shiftRegWidth;
	UINT8 negate;
	UINT8 stereo;
	UINT8 clkDiv;
	UINT8 segaPSG;
	
	void* t6w28_tone;	// SN-chip that emulates the "tone" part of a T6W28
} SN76496_CFG;

extern const DEV_DEF* devDefList_SN76496[];

#endif	// __SN764INTF_H__
