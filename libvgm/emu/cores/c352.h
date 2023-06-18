#ifndef __C352_H__
#define __C352_H__

#include "../EmuStructs.h"

// cfg.flags: 0 = enable all speakers, 1 = disable rear speakers
#define OPT_C352_MUTE_REAR	0x01	// mute rear speakers regardless of configuration
									// (default: disabled)

// default option bitmask: 0x00


extern const DEV_DEF* devDefList_C352[];

#endif	// __C352_H__
