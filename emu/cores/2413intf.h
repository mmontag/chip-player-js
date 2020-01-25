#ifndef __2413INTF_H__
#define __2413INTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_YM2413_MAME		// enable YM2413 core from MAME
#define EC_YM2413_EMU2413	// enable EMU2413 core
#define EC_YM2413_NUKED		// enable Nuked OPNLL
#endif

// cfg.flags: 0 = YM2413 mode, 1 = VRC7 mode

extern const DEV_DEF* devDefList_YM2413[];

#endif	// __2413INTF_H__
