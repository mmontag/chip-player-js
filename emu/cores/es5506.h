#ifndef __ES5506_H__
#define __ES5506_H__

#include "../EmuStructs.h"

typedef struct es5506_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 channels;	// output channels (ES5505: 1..4, ES5506: 1..6, downmixed to stereo)
} ES5506_CFG;

extern const DEV_DEF* devDefList_ES5506[];

#endif	// __ES5506_H__
