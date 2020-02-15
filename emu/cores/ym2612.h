#ifndef __YM2612_H__
#define __YM2612_H__

#include "../../stdtype.h"

typedef struct ym2612__ ym2612_;

ym2612_ *YM2612_Init(UINT32 clock, UINT32 rate, UINT8 interpolation);
void YM2612_End(ym2612_ *YM2612);
void YM2612_Reset(ym2612_ *YM2612);
UINT8 YM2612_Read(ym2612_ *YM2612, UINT8 adr);
void YM2612_Write(ym2612_ *YM2612, UINT8 adr, UINT8 data);
void YM2612_ClearBuffer(DEV_SMPL **buffer, UINT32 length);
void YM2612_Update(ym2612_ *YM2612, DEV_SMPL **buf, UINT32 length);

UINT32 YM2612_GetMute(ym2612_ *YM2612);
void YM2612_SetMute(ym2612_ *YM2612, UINT32 val);
void YM2612_SetOptions(ym2612_ *YM2612, UINT32 Flags);

void YM2612_DacAndTimers_Update(ym2612_ *YM2612, DEV_SMPL **buffer, UINT32 length);
void YM2612_Special_Update(ym2612_ *YM2612);

#endif	// __YM2612_H__
