#ifndef __C140_H__
#define __C140_H__

#include "../EmuStructs.h"

enum
{
	C140_TYPE_SYSTEM2,
	C140_TYPE_SYSTEM21,
	C140_TYPE_ASIC219
};

typedef struct c140_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 banking_type;
} C140_CFG;

extern const DEV_DEF* devDefList_C140[];

#endif	// __C140_H__
