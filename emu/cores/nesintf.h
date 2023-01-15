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


// [NSFPlay core] APU/DMC options
#define OPT_NES_UNMUTE_ON_RESET		0x0001	// enable all channels by default after reset (default: enabled)
#define OPT_NES_NONLINEAR_MIXER		0x0002	// nonlinear mixing (default: enabled)
// [NSFPlay core] APU options
#define OPT_NES_PHASE_REFRESH		0x0004	// writing 4003/4007 resets the square's waveform (default: enabled)
#define OPT_NES_DUTY_SWAP			0x0008	// swap 25:75 and 50:50 duty cycles (behaviour of some Famiclones)
											// (default: disabled)
//#define OPT_NES_NEGATE_SWEEP_INIT	0x00##	// initialize sweep unit unmuted (default: disabled)
// [NSFPlay core] DMC options
#define OPT_NES_ENABLE_4011			0x0010	// enable register 4011 writes (default: enabled)
#define OPT_NES_ENABLE_PNOISE		0x0020	// enable "loop noise" (disable for early Famicom consoles + Arcade boards)
											// (default: enabled)
#define OPT_NES_DPCM_ANTI_CLICK		0x0040	// nullify register 4011 writes, but preserve nonlinearity
											// (default: disabled)
#define OPT_NES_RANDOMIZE_NOISE		0x0080	// randomize noise at device reset (default: enabled)
#define OPT_NES_TRI_MUTE			0x0100	// stops Triangle wave if set to freq = 0,
											// processes it at a very high rate else (default: enabled)
#define OPT_NES_TRI_NULL			0x0200	// always make Triangle return to null-level when stopping,
											// prevents clicking (Valley Bell mod) (default: disabled)
//#define OPT_NES_RANDOMIZE_TRI		0x0###	// randomize triangle on reset (default: enabled)
//#define OPT_NES_DPCM_REVERSE		0x0###	// reverse bits of DPCM sample bytes (default: disabled)
// [NSFPlay FDS core] options
#define OPT_NES_4085_RESET			0x0400	// OPT_4085_RESET (default: disabled)
#define OPT_NES_FDS_DISABLE			0x0800	// OPT_WRITE_PROTECT (default: disabled)

// default option bitmask: 0x01B7
//	OPT_NES_UNMUTE_ON_RESET | OPT_NES_NONLINEAR_MIXER | OPT_NES_PHASE_REFRESH |
//	OPT_NES_ENABLE_4011 | OPT_NES_ENABLE_PNOISE | OPT_NES_RANDOMIZE_NOISE | OPT_NES_TRI_MUTE


extern const DEV_DEF* devDefList_NES_APU[];

#endif	// __NESINTF_H__
