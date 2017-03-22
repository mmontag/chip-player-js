#ifndef __NP_NES_APU_H__
#define __NP_NES_APU_H__

void NES_APU_np_FrameSequence(void* chip, int s);
void* NES_APU_np_Create(UINT32 clock, UINT32 rate);
void NES_APU_np_Destroy(void* chip);
void NES_APU_np_Reset(void* chip);
bool NES_APU_np_Read(void* chip, UINT16 adr, UINT8* val);
bool NES_APU_np_Write(void* chip, UINT16 adr, UINT8 val);
UINT32 NES_APU_np_Render(void* chip, INT32 b[2]);
void NES_APU_np_SetRate(void* chip, UINT32 rate);
void NES_APU_np_SetClock(void* chip, UINT32 clock);
void NES_APU_np_SetOption(void* chip, int id, int b);
void NES_APU_np_SetMask(void* chip, int m);
void NES_APU_np_SetStereoMix(void* chip, int trk, INT16 mixl, INT16 mixr);

#endif	// __NP_NES_APU_H__
