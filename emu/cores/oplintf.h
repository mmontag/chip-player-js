#ifndef __OPLINTF_H__
#define __OPLINTF_H__

#include "../EmuStructs.h"

#if ! defined(SNDDEV_SELECT) && defined(SNDDEV_YM3812)
// undefine one of the variables to disable the cores
#define EC_YM3812_MAME		// enable YM3812 core from MAME
#define EC_YM3812_ADLIBEMU	// enable AdLibEmu core (from DOSBox)
#define EC_YM3812_NUKED		// enable Nuked OPL3 core
#endif


#ifdef SNDDEV_YM3812
extern const DEV_DEF* devDefList_YM3812[];
#endif
#ifdef SNDDEV_YM3526
extern const DEV_DEF* devDefList_YM3526[];
#endif
#ifdef SNDDEV_Y8950
extern const DEV_DEF* devDefList_Y8950[];
#endif

#endif	// __OPLINTF_H__
