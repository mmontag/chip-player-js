#ifndef __ES5503_H__
#define __ES5503_H__

#include "../EmuStructs.h"

typedef struct es5503_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 channels;	// output channels (1..16, downmixed to stereo)
} ES5503_CFG;

extern const DEV_DEF* devDefList_ES5503[];

#endif	// __ES5503_H__
