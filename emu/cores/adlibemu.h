#ifndef __ADLIBEMU_H__
#define __ADLIBEMU_H__

#include "../../stdtype.h"
#include "../snddef.h"

#if defined(OPLTYPE_IS_OPL2)
#define ADLIBEMU(name)			adlib_OPL2_##name
#elif defined(OPLTYPE_IS_OPL3)
#define ADLIBEMU(name)			adlib_OPL3_##name
#endif

typedef void (*ADL_UPDATEHANDLER)(void *param);

void* ADLIBEMU(init)(UINT32 clock, UINT32 samplerate);
void ADLIBEMU(stop)(void *chip);
void ADLIBEMU(reset)(void *chip);

void ADLIBEMU(writeIO)(void *chip, UINT8 addr, UINT8 val);
void ADLIBEMU(getsample)(void *chip, UINT32 numsamples, DEV_SMPL ** sndptr);

UINT8 ADLIBEMU(reg_read)(void *chip, UINT8 port);

void ADLIBEMU(set_update_handler)(void *chip, ADL_UPDATEHANDLER UpdateHandler, void* param);
void ADLIBEMU(set_mute_mask)(void *chip, UINT32 MuteMask);

void ADLIBEMU(set_volume)(void *chip, INT32 volume);
void ADLIBEMU(set_volume_lr)(void *chip, INT32 volL, INT32 volR);

#endif	// __ADLIBEMU_H__
