#ifndef __262INTF_H__
#define __262INTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_YMF262_MAME		// enable YMF262 core from MAME
#define EC_YMF262_ADLIBEMU	// enable AdLibEmu core (from DOSBox)


extern const DEV_DEF* devDefList_YMF262[];

#endif	// __262INTF_H__
