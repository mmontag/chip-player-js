#ifndef __OPNINTF_H__
#define __OPNINTF_H__

#include "../EmuStructs.h"

// YM2610 cfg.flags: 0 = YM2610 mode (4 FM channels), 1 = YM2610B mode (6 FM channels)

#ifdef SNDDEV_YM2203
extern const DEV_DEF* devDefList_YM2203[];
#endif
#ifdef SNDDEV_YM2608
extern const DEV_DEF* devDefList_YM2608[];
#endif
#ifdef SNDDEV_YM2610
extern const DEV_DEF* devDefList_YM2610[];
#endif

#endif	// __OPNINTF_H__
