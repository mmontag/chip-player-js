#ifndef __2151INTF_H__
#define __2151INTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_YM2151_MAME		// enable YM2151 core from MAME
#define EC_YM2151_NUKED		// enable Nuked OPM
#endif


extern const DEV_DEF* devDefList_YM2151[];

#endif	// __2151INTF_H__
