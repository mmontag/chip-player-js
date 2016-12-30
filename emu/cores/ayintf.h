#ifndef __AYINTF_H__
#define __AYINTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_AY8910_MAME		// enable AY8910 core from MAME
#define EC_AY8910_EMU2149	// enable EMU2149 core (from NSFPlay)


#define YM2149_PIN26_HIGH	0x00	// or not connected
#define YM2149_PIN26_LOW	0x10
typedef struct ay8910_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 chipType;
	UINT8 chipFlags;
} AY8910_CFG;

extern const DEV_DEF* devDefList_AY8910[];

#endif	// __AYINTF_H__
