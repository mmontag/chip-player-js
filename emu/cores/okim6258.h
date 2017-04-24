#ifndef __OKIM6258_H__
#define __OKIM6258_H__

#include "../EmuStructs.h"

#define OKIM6258_DIV_1024	0
#define OKIM6258_DIV_768	1
#define OKIM6258_DIV_512	2

#define OKIM6258_ADPCM_3B	3
#define OKIM6258_ADPCM_4B	4

#define	OKIM6258_OUT_10B	10
#define	OKIM6258_OUT_12B	12

typedef struct okim6258_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 divider;		// clock divider, 0 = /1024, 1 = /768, 2 = /512
	UINT8 adpcmBits;	// bits per ADPCM sample (3, 4), 0 = default (4)
	UINT8 outputBits;	// DAC output precision bits (10, 12), 0 = default (10)
} OKIM6258_CFG;


#define OPT_OKIM6258_FORCE_12BIT	0x01	// enforce 12-bit output precision (default: disabled)

// default option bitmask: 0x00


extern const DEV_DEF* devDefList_OKIM6258[];

#endif	// __OKIM6258_H__
