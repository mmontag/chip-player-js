#ifndef __NUKEDOPL3_H__
#define __NUKEDOPL3_H__

#include "../../stdtype.h"
#include "../snddef.h"

void nukedopl3_write(void *chip, UINT8 a, UINT8 v);
UINT8 nukedopl3_read(void *chip, UINT8 a);
void* nukedopl3_init(UINT32 clock, UINT32 rate);
void nukedopl3_shutdown(void *chip);
void nukedopl3_reset_chip(void *chip);
void nukedopl3_update(void *chip, UINT32 samples, DEV_SMPL **out);
void nukedopl3_set_mute_mask(void *chip, UINT32 MuteMask);
void nukedopl3_set_volume(void *chip, INT32 volume);
void nukedopl3_set_vol_lr(void *chip, INT32 volLeft, INT32 volRight);

#endif	// __NUKEDOPL3_H__
