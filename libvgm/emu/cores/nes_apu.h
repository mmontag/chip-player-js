#ifndef __NES_APU_H__
#define __NES_APU_H__

typedef struct _nesapu_state nesapu_state;

UINT8 nes_apu_read(void* chip, UINT8 address);
void nes_apu_write(void* chip, UINT8 offset, UINT8 data);

void nes_apu_update(void* chip, UINT32 samples, DEV_SMPL **outputs);
void* device_start_nesapu(UINT32 clock, UINT32 rate);
void device_stop_nesapu(void* chip);
void device_reset_nesapu(void* chip);

void nesapu_set_rom(void* chip, const UINT8* ROMData);

void nesapu_set_mute_mask(void* chip, UINT32 MuteMask);
UINT32 nesapu_get_mute_mask(void* chip);
void nesapu_set_options(void *chip, UINT32 Flags);
void nesapu_set_panning(void* chip, INT16 square1, INT16 square2, INT16 triangle, INT16 noise, INT16 dpcm);

#endif	// __NES_APU_H__
