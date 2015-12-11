#ifndef __SN764INTF_H__
#define __SN764INTF_H__

#include "EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_MAME		0x00	// SN76496 core from MAME
#define EC_MAXIM	0x01	// SN76489 core by Maxim (from in_vgm)


typedef struct sn76496_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 shiftRegWidth;
	UINT8 noiseTaps;
	UINT8 negate;
	UINT8 stereo;
	UINT8 clkDiv;
	UINT8 segaPSG;
} SN76496_CFG;

UINT8 device_start_sn76496(const SN76496_CFG* cfg, DEV_INFO* retDevInf);

#endif	// __SN764INTF_H__
