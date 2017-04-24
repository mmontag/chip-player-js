#ifndef __GB_H__
#define __GB_H__

#include "../EmuStructs.h"


#define OPT_GB_DMG_BOOST_WAVECH	0x01	// boost volume of wave channel by 6 db
										// (default: disabled)
#define OPT_GB_DMG_LEGACY_MODE	0x80	// simulate behaviour of old MAME core
										// required for playing old, optimized VGM files
										// (default: disabled)

// default option bitmask: 0x00


extern const DEV_DEF* devDefList_GB_DMG[];

#endif	// __GB_H__
