#ifndef __RF5C68_H__
#define __RF5C68_H__

#include <stdtype.h>
#include "../snddef.h"

typedef void (*SAMPLE_END_CB)(void* param,int channel);

void rf5c68_update(void *info, UINT32 samples, DEV_SMPL **outputs);

void* device_start_rf5c68(UINT32 clock);
void device_stop_rf5c68(void *info);
void device_reset_rf5c68(void *info);
void rf5c68_set_sample_end_callback(void *info, SAMPLE_END_CB callback, void* param);

UINT8 rf5c68_r(void *info, UINT8 offset);
void rf5c68_w(void *info, UINT8 offset, UINT8 data);

UINT8 rf5c68_mem_r(void *info, UINT16 offset);
void rf5c68_mem_w(void *info, UINT16 offset, UINT8 data);
void rf5c68_write_ram(void *info, UINT32 offset, UINT32 length, const UINT8* data);

void rf5c68_set_mute_mask(void *info, UINT32 MuteMask);

#endif	// __RF5C68_H__
