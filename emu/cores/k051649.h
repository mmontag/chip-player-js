#ifndef __K051649_H__
#define __K051649_H__

#include "../EmuStructs.h"

// cfg.flags: 0 = SCC mode (K051649), 1 = SCC+ mode (K052539)

extern const DEV_DEF* devDefList_K051649[];

// K051649 read/write offsets:
//	0x00 - waveform
//	0x01 - frequency
//	0x02 - volume
//	0x03 - key on/off
//	0x04 - waveform (0x00 used to do SCC access, 0x04 SCC+)
//	0x05 - test register

#endif	// __K051649_H__
