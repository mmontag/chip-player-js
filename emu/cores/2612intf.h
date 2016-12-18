#ifndef __2612INTF_H__
#define __2612INTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_YM2612_GPGX		// enable YM2612 core from MAME/Genesis Plus GX
#define EC_YM2612_GENS		// enable Gens YM2612 core (from in_vgm)


extern const DEV_DEF* devDefList_YM2612[];

#endif	// __2612INTF_H__
