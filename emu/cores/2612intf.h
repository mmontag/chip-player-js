#ifndef __2612INTF_H__
#define __2612INTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_YM2612_GPGX		// enable YM2612 core from MAME/Genesis Plus GX
#define EC_YM2612_GENS		// enable Gens YM2612 core (from in_vgm)
#define EC_YM2612_NUKED		// enable Nuked YM3438/YM2612 core
#endif


#define OPT_YM2612_DAC_HIGHPASS		0x01	// [Gens core] enable DAC highpass filter (default: disabled)
#define OPT_YM2612_SSGEG			0x02	// [Gens core] enable (broken) SSG-EG emulation (default: disabled)
#define OPT_YM2612_PSEUDO_STEREO	0x04	// [GPGX core] Pseudo Stereo mode (default: disabled)
											// alternate between updating left and right speaker,
											// adds some stereo effect to sharp and noisy sounds
											// !! double chip update rate for proper sound
#define OPT_YM2612_TYPE_OPN2		0x00	// [Nuked OPN2] emulate YM2612
#define OPT_YM2612_TYPE_OPN2C_ASIC	0x10	// [Nuked OPN2] emulate ASIC YM3438
#define OPT_YM2612_TYPE_OPN2C_DISC	0x20	// [Nuked OPN2] emulate Discrete YM3438
#define OPT_YM2612_LEGACY_MODE		0x80	// [GPGX core] simulate behaviour of older emulation cores
											// not recommended, but required for playing GYM files
											// (default: disabled)

// default option bitmask: 0x00


extern const DEV_DEF* devDefList_YM2612[];

#endif	// __2612INTF_H__
