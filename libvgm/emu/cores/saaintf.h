#ifndef __SAAINTF_H__
#define __SAAINTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_SAA1099_MAME		// enable SAA1099 core from MAME
#define EC_SAA1099_NRS		// enable SAA1099 core by NewRisingSun
#define EC_SAA1099_VB		// enable SAA1099 core by Valley Bell
#endif


extern const DEV_DEF* devDefList_SAA1099[];

#endif	// __SAAINTF_H__
