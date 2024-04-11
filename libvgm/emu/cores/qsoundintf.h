#ifndef __QSOUNDINTF_H__
#define __QSOUNDINTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_QSOUND_MAME		// enable QSound core from MAME
#define EC_QSOUND_CTR		// enable QSound core by superctr
#endif


#define OPT_QSOUND_NOWAIT		0x01	// don't require waiting after initialization/filter changes


extern const DEV_DEF* devDefList_QSound[];

#endif	// __QSOUNDINTF_H__
