#ifndef __C6280_MAME_H__
#define __C6280_MAME_H__

#include <stdtype.h>
#include "../snddef.h"

void c6280mame_w(void *chip, UINT8 offset, UINT8 data);
UINT8 c6280mame_r(void* chip, UINT8 offset);

void c6280mame_update(void* param, UINT32 samples, DEV_SMPL **outputs);
void* device_start_c6280mame(UINT32 clock, UINT32 rate);
void device_stop_c6280mame(void* chip);
void device_reset_c6280mame(void* chip);

void c6280mame_set_mute_mask(void* chip, UINT32 MuteMask);

#endif	// __C6280_MAME_H__
