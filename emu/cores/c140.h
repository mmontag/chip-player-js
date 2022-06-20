#ifndef __C140_H__
#define __C140_H__

#include "../EmuStructs.h"

// cfg.flags: Banking Type
#define C140_TYPE_SYSTEM2	0x00	// Namco System 2 banking
#define C140_TYPE_SYSTEM21	0x01	// Namco System 21 banking
//#define C140_TYPE_ASIC219	0x02
#define C140_TYPE_LINEAR	0x03	// linear ROM space (unbanked)

extern const DEV_DEF* devDefList_C140[];

#endif	// __C140_H__
