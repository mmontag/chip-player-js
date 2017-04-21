#ifndef __NESINTF_H__
#define __NESINTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_NES_MAME			// enable NES core from MAME
#define EC_NES_NSFPLAY		// enable NES core from NSFPlay
#define EC_NES_NSFP_FDS		// enable FDS core from NSFPlay
// Note: The FDS core from NSFPlay can be used with both NES cores.
#endif

// cfg.flags: set to 1 to enable FDS sound

extern const DEV_DEF* devDefList_NES_APU[];

#endif	// __NESINTF_H__
