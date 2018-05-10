#ifndef __QSOUNDINTF_H__
#define __QSOUNDINTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_QSOUND_MAME		// enable QSound core from MAME
#define EC_QSOUND_VB		// enable QSound core by Valley Bell
#endif


extern const DEV_DEF* devDefList_QSound[];

#endif	// __QSOUNDINTF_H__
