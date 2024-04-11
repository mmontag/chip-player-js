#ifndef __SEGAPCM_H__
#define __SEGAPCM_H__

#include "../../stdtype.h"
#include "../EmuStructs.h"

// cfg.bnkshift: ROM bank shift
#define	SEGAPCM_BANK_256	11
#define	SEGAPCM_BANK_512	12
#define	SEGAPCM_BANK_12M	13
// cfg.bnkmask: bank register bitmask
#define	SEGAPCM_BANK_MASK7	0x70
#define	SEGAPCM_BANK_MASKF	0xF0
#define	SEGAPCM_BANK_MASKF8	0xF8

typedef struct segapcm_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 bnkshift;
	UINT8 bnkmask;
} SEGAPCM_CFG;

extern const DEV_DEF* devDefList_SegaPCM[];

#endif	// __SEGAPCM_H__
