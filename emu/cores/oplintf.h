#ifndef __OPLINTF_H__
#define __OPLINTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_YM3812_MAME		// enable YM3812 core from MAME
#define EC_YM3812_ADLIBEMU	// enable AdLibEmu core (from DOSBox)
#define EC_YM3812_NUKED		// enable Nuked OPL3 core


extern const DEV_DEF* devDefList_YM3812[];
extern const DEV_DEF* devDefList_YM3526[];
extern const DEV_DEF* devDefList_Y8950[];

#endif	// __OPLINTF_H__
