#ifndef __OKIM6258_H__
#define __OKIM6258_H__

#include "../EmuStructs.h"

#define OKIM6258_DIV_1024	0
#define OKIM6258_DIV_768	1
#define OKIM6258_DIV_512	2

#define OKIM6258_ADPCM_3B	0
#define OKIM6258_ADPCM_4B	1

#define	OKIM6258_OUT_10B	0
#define	OKIM6258_OUT_12B	1

typedef struct okim6258_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 divider;		// clock divider, 0 = /1024, 1 = /768, 2 = /512
	UINT8 adpcmBits;	// bits per ADPCM sample, 0 = 3 bits, 1 = 4 bits
	UINT8 outputBits;	// output precision, 0 = 10 bits, 1 = 12 bits
} OKIM6258_CFG;

extern const DEV_DEF* devDefList_OKIM6258[];

#endif	// __OKIM6258_H__
