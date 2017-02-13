#ifndef __C352_H__
#define __C352_H__

#include "../EmuStructs.h"

// cfg.flags: 0 = enable all speakers, 1 = disable rear speakers

typedef struct C352_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT16 clk_divider;	// master clock divider
} C352_CFG;

extern const DEV_DEF* devDefList_C352[];

#endif	// __C352_H__
