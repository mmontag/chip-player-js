#ifndef __NP_NES_FDS_H__
#define __NP_NES_FDS_H__

void* NES_FDS_Create(UINT32 clock, UINT32 rate);
void NES_FDS_Destroy(void* chip);
void NES_FDS_Reset(void* chip);
UINT32 NES_FDS_Render(void* chip, INT32 b[2]);
bool NES_FDS_Write(void* chip, UINT16 adr, UINT8 val);
bool NES_FDS_Read(void* chip, UINT16 adr, UINT8* val);
void NES_FDS_SetRate(void* chip, UINT32 r);
void NES_FDS_SetClock(void* chip, UINT32 c);
void NES_FDS_SetOption(void* chip, int id, int val);
int NES_FDS_GetOption(void* chip, int id);
void NES_FDS_SetMask(void* chip, int m);
void NES_FDS_SetStereoMix(void* chip, int trk, INT16 mixl, INT16 mixr);

#endif	// __NP_NES_FDS_H__
