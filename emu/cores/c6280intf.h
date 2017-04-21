#ifndef __C6280INTF_H__
#define __C6280INTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_C6280_MAME		// enable HuC6280 core from MAME
#define EC_C6280_OOTAKE		// enable Ootake PSG core
#endif

// cfg.flags: [Ootake core] set to 1 enable a patch for "Hany in the Sky"

extern const DEV_DEF* devDefList_C6280[];

#endif	// __C6280INTF_H__
