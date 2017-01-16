#ifndef __K054539_H__
#define __K054539_H__

#include "../EmuStructs.h"

// control flags
#define K054539_RESET_FLAGS     0
#define K054539_REVERSE_STEREO  1
#define K054539_DISABLE_REVERB  2
#define K054539_UPDATE_AT_KEYON 4

typedef struct k054539_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 flags;
} K054539_CFG;

extern const DEV_DEF* devDefList_K054539[];

#endif	// __K054539_H__
