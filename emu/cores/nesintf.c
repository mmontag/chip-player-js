#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../../stdbool.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "nesintf.h"
#include "../panning.h"

#ifdef EC_NES_MAME
#include "nes_apu.h"
#endif
#ifdef EC_NES_NSFPLAY
#include "np_nes_apu.h"
#include "np_nes_dmc.h"
#endif
#ifdef EC_NES_NSFP_FDS
#include "np_nes_fds.h"
#define FDS_OPT_WRITE_PROTECT	2
#endif


typedef struct nesapu_info NESAPU_INF;

static void nes_stream_update_mame(void* chip, UINT32 samples, DEV_SMPL** outputs);
static void nes_stream_update_nsfplay(void* chip, UINT32 samples, DEV_SMPL** outputs);
static void nes_render_fds(void* chip_fds, UINT32 samples, DEV_SMPL** outputs);

static UINT8 device_start_nes_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_nes_nsfplay(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_nes_mame(void* chip);
static void device_stop_nes_nsfplay(void* chip);
static void device_stop_nes_common(NESAPU_INF* info);
static void device_reset_nes_mame(void* chip);
static void device_reset_nes_nsfplay(void* chip);

static void nes_w_mame(void* chip, UINT8 offset, UINT8 data);
static UINT8 nes_r_mame(void* chip, UINT8 offset);
static void nes_w_nsfplay(void* chip, UINT8 offset, UINT8 data);
static UINT8 nes_r_nsfplay(void* chip, UINT8 offset);
static UINT8 nes_read_fds(void* chip_fds, UINT8 offset);
static void nes_write_ram(void* chip, UINT32 offset, UINT32 length, const UINT8* data);

static void nes_set_chip_option_mame(void* chip, UINT32 NesOptions);
static void nes_set_chip_option_nsfplay(void* chip, UINT32 NesOptions);
static void nes_set_chip_option_fds(NESAPU_INF* info, UINT32 NesOptions);
static void nes_set_mute_mask_mame(void* chip, UINT32 MuteMask);
static void nes_set_pan_mame(void* chipptr, const INT16* PanVals);
static void nes_set_mute_mask_nsfplay(void* chip, UINT32 MuteMask);
static void nes_set_pan_nsfplay(void* chip, const INT16* PanVals);


#ifdef EC_NES_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nes_w_mame},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, nes_r_mame},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, nes_write_ram},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, nes_set_mute_mask_mame},
	{RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, nes_set_pan_mame},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"NES APU", "MAME", FCC_MAME,
	
	device_start_nes_mame,
	device_stop_nes_mame,
	device_reset_nes_mame,
	nes_stream_update_mame,
	
	nes_set_chip_option_mame,	// SetOptionBits
	nes_set_mute_mask_mame,
	nes_set_pan_mame,
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_NES_NSFPLAY
static DEVDEF_RWFUNC devFunc_NSFPlay[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nes_w_nsfplay},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, nes_r_nsfplay},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, nes_write_ram},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, nes_set_mute_mask_nsfplay},
	{RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, nes_set_pan_nsfplay},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_NSFPlay =
{
	"NES APU", "NSFPlay", FCC_NSFP,
	
	device_start_nes_nsfplay,
	device_stop_nes_nsfplay,
	device_reset_nes_nsfplay,
	nes_stream_update_nsfplay,
	
	nes_set_chip_option_nsfplay,	// SetOptionBits
	nes_set_mute_mask_nsfplay,
	nes_set_pan_nsfplay,
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc_NSFPlay,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_NES_APU[] =
{
#ifdef EC_NES_NSFPLAY
	&devDef_NSFPlay,
#endif
#ifdef EC_NES_MAME
	&devDef_MAME,
#endif
	NULL
};


struct nesapu_info
{
	DEV_DATA _devData;
	
	void* chip_apu;
	void* chip_dmc;
	void* chip_fds;
	UINT8* memory;
	UINT8 fds_disable;
};

// bit shift for transforming the "usual" panning values (factor 1<<16) into
// NSFPlay's scale, which uses a factor of 1<<7
#define PAN_NSFSHIFT	(PANNING_BITS - 7)
#define PAN_EMU2NSF(x)	(INT16)((x + (1 << PAN_NSFSHIFT >> 1)) >> PAN_NSFSHIFT)	// (x >> PAN_NSFSHIFT) with additional rounding


#ifdef EC_NES_MAME
static void nes_stream_update_mame(void* chip, UINT32 samples, DEV_SMPL** outputs)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	nes_apu_update(info->chip_apu, samples, outputs);
	
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		nes_render_fds(info->chip_fds, samples, outputs);
#endif
	
	return;
}
#endif

#ifdef EC_NES_NSFPLAY
static void nes_stream_update_nsfplay(void* chip, UINT32 samples, DEV_SMPL** outputs)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	UINT32 CurSmpl;
	INT32 Buffer[4];
	
	for (CurSmpl = 0; CurSmpl < samples; CurSmpl ++)
	{
		NES_APU_np_Render(info->chip_apu, &Buffer[0]);
		NES_DMC_np_Render(info->chip_dmc, &Buffer[2]);
		outputs[0][CurSmpl] = Buffer[0] + Buffer[2];
		outputs[1][CurSmpl] = Buffer[1] + Buffer[3];
	}
	
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		nes_render_fds(info->chip_fds, samples, outputs);
#endif
	
	return;
}
#endif

#ifdef EC_NES_NSFP_FDS
static void nes_render_fds(void* chip_fds, UINT32 samples, DEV_SMPL** outputs)
{
	UINT32 CurSmpl;
	INT32 Buffer[2];
	
	for (CurSmpl = 0; CurSmpl < samples; CurSmpl ++)
	{
		NES_FDS_Render(chip_fds, &Buffer[0]);
		outputs[0][CurSmpl] += Buffer[0];
		outputs[1][CurSmpl] += Buffer[1];
	}
	
	return;
}
#endif

#ifdef EC_NES_MAME
static UINT8 device_start_nes_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	NESAPU_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 4;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	info = (NESAPU_INF*)calloc(1, sizeof(NESAPU_INF));
	if (info == NULL)
		return 0xFF;
	info->chip_apu = device_start_nesapu(cfg->clock, rate);
	if (info->chip_apu == NULL)
	{
		free(info);
		return 0xFF;
	}
	info->chip_dmc = NULL;
	
#ifdef EC_NES_NSFP_FDS
	if (cfg->flags)	// enable FDS sound
		info->chip_fds = NES_FDS_Create(cfg->clock, rate);
	else
#endif
		info->chip_fds = NULL;
	
	info->memory = (UINT8*)malloc(0x8000);
	memset(info->memory, 0x00, 0x8000);
	nesapu_set_rom(info->chip_apu, info->memory - 0x8000);
	
	info->fds_disable = 0;
	
	// store pointer to NESAPU_INF into sound chip structures
	info->_devData.chipInf = info;
	devData = (DEV_DATA*)info->chip_apu;
	devData->chipInf = info;
	if (info->chip_fds != NULL)
	{
		devData = (DEV_DATA*)info->chip_fds;
		devData->chipInf = info;
	}
	INIT_DEVINF(retDevInf, &info->_devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_NES_NSFPLAY
static UINT8 device_start_nes_nsfplay(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	NESAPU_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 4;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	info = (NESAPU_INF*)calloc(1, sizeof(NESAPU_INF));
	if (info == NULL)
		return 0xFF;
	info->chip_apu = NES_APU_np_Create(cfg->clock, rate);
	if (info->chip_apu == NULL)
	{
		free(info);
		return 0xFF;
	}
	info->chip_dmc = NES_DMC_np_Create(cfg->clock, rate);
	if (info->chip_dmc == NULL)
	{
		NES_APU_np_Destroy(info->chip_apu);
		free(info);
		return 0xFF;
	}
	NES_DMC_np_SetAPU(info->chip_dmc, info->chip_apu);
	
#ifdef EC_NES_NSFP_FDS
	if (cfg->flags)	// enable FDS sound
		info->chip_fds = NES_FDS_Create(cfg->clock, rate);
	else
#endif
		info->chip_fds = NULL;
	
	info->memory = (UINT8*)malloc(0x8000);
	memset(info->memory, 0x00, 0x8000);
	NES_DMC_np_SetMemory(info->chip_dmc, info->memory - 0x8000);
	
	info->fds_disable = 0;
	
	// store pointer to NESAPU_INF into sound chip structures
	info->_devData.chipInf = info;
	devData = (DEV_DATA*)info->chip_apu;
	devData->chipInf = info;
	devData = (DEV_DATA*)info->chip_dmc;
	devData->chipInf = info;
	if (info->chip_fds != NULL)
	{
		devData = (DEV_DATA*)info->chip_fds;
		devData->chipInf = info;
	}
	INIT_DEVINF(retDevInf, &info->_devData, rate, &devDef_NSFPlay);
	return 0x00;
}
#endif

#ifdef EC_NES_MAME
static void device_stop_nes_mame(void* chip)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	device_stop_nesapu(info->chip_apu);
	device_stop_nes_common(info);
	
	return;
}
#endif

#ifdef EC_NES_NSFPLAY
static void device_stop_nes_nsfplay(void* chip)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	NES_APU_np_Destroy(info->chip_apu);
	NES_DMC_np_Destroy(info->chip_dmc);
	device_stop_nes_common(info);
	
	return;
}
#endif

static void device_stop_nes_common(NESAPU_INF* info)
{
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_Destroy(info->chip_fds);
#endif
	if (info->memory != NULL)
		free(info->memory);
	
	free(info);
	return;
}

#ifdef EC_NES_MAME
static void device_reset_nes_mame(void* chip)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	device_reset_nesapu(info->chip_apu);
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_Reset(info->chip_fds);
#endif
}
#endif

#ifdef EC_NES_NSFPLAY
static void device_reset_nes_nsfplay(void* chip)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	NES_APU_np_Reset(info->chip_apu);
	NES_DMC_np_Reset(info->chip_dmc);
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_Reset(info->chip_fds);
#endif
	
	return;
}
#endif


#ifdef EC_NES_MAME
static void nes_w_mame(void* chip, UINT8 offset, UINT8 data)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	if (offset < 0x20)	// NES APU
	{
		nes_apu_write(info->chip_apu, offset, data);
	}
	else	// FDS
	{
#ifdef EC_NES_NSFP_FDS
		if (info->chip_fds != NULL && ! info->fds_disable)
			NES_FDS_Write(info->chip_fds, 0x4000 | offset, data);
#endif
	}
	return;
}

static UINT8 nes_r_mame(void* chip, UINT8 offset)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	if (offset < 0x20)	// NES APU
	{
		return nes_apu_read(info->chip_apu, offset);
	}
	else	// FDS
	{
#ifdef EC_NES_NSFP_FDS
		if (info->chip_fds != NULL && ! info->fds_disable)
			return nes_read_fds(info->chip_fds, offset);
#endif
		return 0x00;
	}
}
#endif

#ifdef EC_NES_NSFPLAY
static void nes_w_nsfplay(void* chip, UINT8 offset, UINT8 data)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	if (offset < 0x20)	// NES APU
	{
		// NES_APU handles the sqaure waves, NES_DMC the rest
		NES_APU_np_Write(info->chip_apu, 0x4000 | offset, data);
		NES_DMC_np_Write(info->chip_dmc, 0x4000 | offset, data);
	}
	else	// FDS
	{
#ifdef EC_NES_NSFP_FDS
		if (info->chip_fds != NULL && ! info->fds_disable)
			NES_FDS_Write(info->chip_fds, 0x4000 | offset, data);
#endif
	}
	return;
}

static UINT8 nes_r_nsfplay(void* chip, UINT8 offset)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	if (offset < 0x20)	// NES APU
	{
		UINT8 readVal = 0x00;
		
		// the functions OR the result to the value
		NES_APU_np_Read(info, 0x4000 | offset, &readVal);
		NES_DMC_np_Read(info, 0x4000 | offset, &readVal);
		
		return readVal;
	}
	else	// FDS
	{
#ifdef EC_NES_NSFP_FDS
		if (info->chip_fds != NULL && ! info->fds_disable)
			return nes_read_fds(info->chip_fds, offset);
#endif
		return 0x00;
	}
}
#endif

#ifdef EC_NES_NSFP_FDS
static UINT8 nes_read_fds(void* chip_fds, UINT8 offset)
{
	bool success;
	UINT8 readVal;
	
	success = NES_FDS_Read(chip_fds, 0x4000 | offset, &readVal);
	if (success)
		return readVal;
	
	return 0x00;
}
#endif

static void nes_write_ram(void* chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	UINT32 remBytes;
	
	if (offset >= 0x10000)
		return;
	
	// if (offset < 0x8000), then just copy the part for 0x8000+
	if (offset < 0x8000)
	{
		if (offset + length <= 0x8000)
			return;
		
		remBytes = 0x8000 - offset;
		offset = 0x8000;
		data += remBytes;
		length -= remBytes;
	}
	
	remBytes = 0x0000;
	if (offset + length > 0x10000)
	{
		remBytes = length;
		length = 0x10000 - offset;
		remBytes -= length;
	}
	memcpy(info->memory + (offset - 0x8000), data, length);
	// if we're crossing the boundary for 0x10000,
	// copy the bytes for 0x10000+ to 0x8000+
	if (remBytes)
	{
		if (remBytes > 0x8000)
			remBytes = 0x8000;
		memcpy(info->memory, data + length, remBytes);
	}
	
	return;
}


#ifdef EC_NES_MAME
static void nes_set_chip_option_mame(void* chip, UINT32 NesOptions)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	nesapu_set_options(info->chip_apu, NesOptions);
	
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		nes_set_chip_option_fds(info, NesOptions);
#endif
	
	return;
}
#endif

#ifdef EC_NES_NSFPLAY
static void nes_set_chip_option_nsfplay(void* chip, UINT32 NesOptions)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	UINT8 CurOpt;
	
	// shared APU/DMC options
	for (CurOpt = 0; CurOpt < 2; CurOpt ++)
	{
		NES_APU_np_SetOption(info->chip_apu, CurOpt, (NesOptions >> CurOpt) & 0x01);
		NES_DMC_np_SetOption(info->chip_dmc, CurOpt, (NesOptions >> CurOpt) & 0x01);
	}
	// APU-only options
	for (; CurOpt < 4; CurOpt ++)
		NES_APU_np_SetOption(info->chip_apu, CurOpt-2+2, (NesOptions >> CurOpt) & 0x01);
	// DMC-only options
	for (; CurOpt < 10; CurOpt ++)
		NES_DMC_np_SetOption(info->chip_dmc, CurOpt-4+2, (NesOptions >> CurOpt) & 0x01);
	
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		nes_set_chip_option_fds(info, NesOptions);
#endif
	
	return;
}
#endif

#ifdef EC_NES_NSFP_FDS
static void nes_set_chip_option_fds(NESAPU_INF* info, UINT32 NesOptions)
{
	UINT8 CurOpt;
	
	// FDS options
	// I skip the Cutoff frequency here, since it's not a boolean value.
	for (CurOpt = 10; CurOpt < 12; CurOpt ++)
		NES_FDS_SetOption(info->chip_fds, CurOpt-10+1, (NesOptions >> CurOpt) & 0x01);
	info->fds_disable = NES_FDS_GetOption(info->chip_fds, FDS_OPT_WRITE_PROTECT);
	
	return;
}
#endif

#ifdef EC_NES_MAME
static void nes_set_mute_mask_mame(void* chip, UINT32 MuteMask)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	nesapu_set_mute_mask(info->chip_apu, MuteMask);
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_SetMask(info->chip_fds, (MuteMask & 0x20) >> 5);
#endif
	
	return;
}

static void nes_set_pan_mame(void* chip, const INT16* PanVals)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	nesapu_set_panning(info->chip_apu, PanVals[0], PanVals[1], PanVals[2], PanVals[3], PanVals[4]);
	
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
	{
		INT32 fdsPan[2];
		Panning_Calculate(fdsPan, PanVals[5]);
		NES_FDS_SetStereoMix(info->chip_fds, 0, PAN_EMU2NSF(fdsPan[0]), PAN_EMU2NSF(fdsPan[1]));
	}
#endif
	
	return;
}
#endif

#ifdef EC_NES_NSFPLAY
static void nes_set_mute_mask_nsfplay(void* chip, UINT32 MuteMask)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	
	NES_APU_np_SetMask(info->chip_apu, (MuteMask & 0x03) >> 0);
	NES_DMC_np_SetMask(info->chip_dmc, (MuteMask & 0x1C) >> 2);
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_SetMask(info->chip_fds, (MuteMask & 0x20) >> 5);
#endif
	
	return;
}

static void nes_set_pan_nsfplay(void* chip, const INT16* PanVals)
{
	NESAPU_INF* info = (NESAPU_INF*)chip;
	UINT8 curChn;
	INT32 pan[6][2];
	
	for (curChn = 0; curChn < 6; curChn++)
		Panning_Calculate(pan[curChn], PanVals[curChn]);
	
	NES_APU_np_SetStereoMix(info->chip_apu, 0, PAN_EMU2NSF(pan[0][0]), PAN_EMU2NSF(pan[0][1]));
	NES_APU_np_SetStereoMix(info->chip_apu, 1, PAN_EMU2NSF(pan[1][0]), PAN_EMU2NSF(pan[1][1]));
	NES_DMC_np_SetStereoMix(info->chip_dmc, 0, PAN_EMU2NSF(pan[2][0]), PAN_EMU2NSF(pan[2][1]));
	NES_DMC_np_SetStereoMix(info->chip_dmc, 1, PAN_EMU2NSF(pan[3][0]), PAN_EMU2NSF(pan[3][1]));
	NES_DMC_np_SetStereoMix(info->chip_dmc, 2, PAN_EMU2NSF(pan[4][0]), PAN_EMU2NSF(pan[4][1]));
#ifdef EC_NES_NSFP_FDS
	if (info->chip_fds != NULL)
		NES_FDS_SetStereoMix(info->chip_fds, 0, PAN_EMU2NSF(pan[5][0]), PAN_EMU2NSF(pan[5][1]));
#endif
	
	return;
}
#endif
