#ifndef __YM2413_H__
#define __YM2413_H__

#include "../EmuStructs.h"
#include "../../stdtype.h"

extern DEV_DEF devDef_YM2413_MAME;

typedef void (*OPLL_UPDATEHANDLER)(void *param,int min_interval_us);

void ym2413_set_update_handler(void *chip, OPLL_UPDATEHANDLER UpdateHandler, void *param);
void ym2413_set_chip_mode(void* chip, UINT8 Mode);
void ym2413_override_patches(void* chip, const UINT8* PatchDump);

#endif	// __YM2413_H__
