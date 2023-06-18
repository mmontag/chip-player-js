#ifndef __SCSP_H__
#define __SCSP_H__

#include "../EmuStructs.h"


#define OPT_SCSP_BYPASS_DSP	0x01	// skip all DSP calculations (huge speedup, DSP is broken anyway)
									// (default: enabled)

// default option bitmask: 0x01


extern const DEV_DEF* devDefList_SCSP[];

#endif	// __SCSP_H__
