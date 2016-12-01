#ifndef __2413INTF_H__
#define __2413INTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_YM2413_MAME		// enable YM2413 core from MAME
#define EC_YM2413_EMU2413	// enable EMU2413 core (from in_vgm)


extern const DEV_DEF* devDefList_YM2413[];

#endif	// __2413INTF_H__
