#ifndef __YM2413_H__
#define __YM2413_H__

#include <stdtype.h>

void *ym2413_init(int clock, int rate);
void ym2413_shutdown(void *chip);
void ym2413_reset_chip(void *chip);
void ym2413_write(void *chip, UINT8 a, UINT8 v);
UINT8 ym2413_read(void *chip, UINT8 a);
void ym2413_update_one(void *chip, UINT32 length, DEV_SMPL **buffers);

typedef void (*OPLL_UPDATEHANDLER)(void *param,int min_interval_us);

void ym2413_set_update_handler(void *chip, OPLL_UPDATEHANDLER UpdateHandler, void *param);
void ym2413_set_mutemask(void* chip, UINT32 MuteMask);
void ym2413_set_chip_mode(void* chip, UINT8 Mode);
void ym2413_override_patches(void* chip, const UINT8* PatchDump);

#endif	// __YM2413_H__
