#ifndef __SAA1099_VB_H__
#define __SAA1099_VB_H__

#include <stdtype.h>
#include "../snddef.h"

void saa1099v_write(void* info, UINT8 offset, UINT8 data);

void saa1099v_update(void* param, UINT32 samples, DEV_SMPL** outputs);
void* saa1099v_create(UINT32 clock, UINT32 sampleRate);
void saa1099v_destroy(void* info);
void saa1099v_reset(void* info);

void saa1099v_set_mute_mask(void* info, UINT32 MuteMask);

#endif	// __SAA1099_VB_H__
