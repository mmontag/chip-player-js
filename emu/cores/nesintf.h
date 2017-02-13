#ifndef __NESINTF_H__
#define __NESINTF_H__

#include "../EmuStructs.h"

// undefine one of the variables to disable the cores
#define EC_NES_MAME		// enable NES core from MAME
#define EC_NES_NSFPLAY		// enable NES core from NSFPlay
// Note: FDS core from NSFPlay is always used

// cfg.flags: set to 1 to enable FDS sound

extern const DEV_DEF* devDefList_NES_APU[];

#endif	// __NESINTF_H__
