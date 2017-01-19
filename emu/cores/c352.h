#ifndef __C352_H__
#define __C352_H__

#include "../EmuStructs.h"

typedef struct C352_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT16 clk_divider;
} C352_CFG;

extern const DEV_DEF* devDefList_C352[];

#endif	// __C352_H__
