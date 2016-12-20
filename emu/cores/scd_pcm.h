#ifndef __SCD_PCM_H__
#define __SCD_PCM_H__

#include <stdtype.h>

void*SCD_PCM_Init(UINT32 Clock, UINT32 Rate, UINT8 smpl0patch);
void SCD_PCM_Deinit(void* info);
void SCD_PCM_Set_Rate(void* info, UINT32 Clock, UINT32 Rate);
void SCD_PCM_Reset(void* info);
void SCD_PCM_Write_Reg(void* info, UINT8 Reg, UINT8 Data);
UINT8 SCD_PCM_Read_Reg(void* info, UINT8 Reg);
void SCD_PCM_Update(void* info, UINT32 Length, DEV_SMPL **buf);
UINT8 SCD_PCM_MemRead(void *info, UINT16 offset);
void SCD_PCM_MemWrite(void *info, UINT16 offset, UINT8 data);
void SCD_PCM_MemBlockWrite(void* info, UINT32 offset, UINT32 length, const UINT8* data);
void SCD_PCM_SetMuteMask(void* info, UINT32 MuteMask);

#endif	// __SCD_PCM_H__
