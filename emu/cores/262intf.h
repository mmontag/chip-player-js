#ifndef __262INTF_H__
#define __262INTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_YMF262_MAME		// enable YMF262 core from MAME
#define EC_YMF262_ADLIBEMU	// enable AdLibEmu core (from DOSBox)
#define EC_YMF262_NUKED		// enable Nuked OPL3 core
#endif


extern const DEV_DEF* devDefList_YMF262[];

#endif	// __262INTF_H__
