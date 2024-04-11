#ifndef __UPD7759_H__
#define __UPD7759_H__

#include "../EmuStructs.h"

// cfg.flags: 0 = master mode (uses ROM), 1 = slave mode (data streamed to chip)

extern const DEV_DEF* devDefList_uPD7759[];

// uPD7759 write offsets:
//	0x00 - reset line
//	0x01 - start line
//	0x02 - data port
//	0x03 - set ROM bank
// uPD7759 read offsets:
//	'F' - get space in Sega Pico FIFO queue
//	else - ready line

#endif	// __UPD7759_H__
