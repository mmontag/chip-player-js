#include <stdlib.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "2612intf.h"
#ifdef EC_YM2612_GPGX
#ifndef SNDDEV_YM2612
#define SNDDEV_YM2612
#endif
#include "fmopn.h"
#endif
#ifdef EC_YM2612_GENS
#include "ym2612.h"
#endif
#ifdef EC_YM2612_NUKED
#include "ym3438.h"
#endif


static void ym2612_gens_update(void* chip, UINT32 samples, DEV_SMPL** outputs);
static UINT8 device_start_ym2612_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym2612_gens(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym2612_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);


#ifdef EC_YM2612_GPGX
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym2612_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym2612_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ym2612_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"YM2612", "GPGX", FCC_GPGX,
	
	device_start_ym2612_mame,
	ym2612_shutdown,
	ym2612_reset_chip,
	ym2612_update_one,
	
	ym2612_set_options,	// SetOptionBits
	ym2612_set_mute_mask,	// SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	ym2612_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_YM2612_GENS
static DEVDEF_RWFUNC devFunc_Gens[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, YM2612_Write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, YM2612_Read},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Gens =
{
	"YM2612", "Gens", FCC_GENS,
	
	device_start_ym2612_gens,
	(DEVFUNC_CTRL)YM2612_End,
	(DEVFUNC_CTRL)YM2612_Reset,
	ym2612_gens_update,
	
	(DEVFUNC_OPTMASK)YM2612_SetOptions,	// SetOptionBits
	(DEVFUNC_OPTMASK)YM2612_SetMute,	// SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc_Gens,	// rwFuncs
};
#endif
#ifdef EC_YM2612_NUKED
static DEVDEF_RWFUNC devFunc_Nuked[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nukedopn2_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, nukedopn2_read},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Nuked =
{
	"YM3438", "Nuked OPN2", FCC_NUKE,
	
	device_start_ym2612_nuked,
	nukedopn2_shutdown,
	nukedopn2_reset_chip,
	nukedopn2_update,
	
	nukedopn2_set_options,	// SetOptionBits
	nukedopn2_set_mute_mask,	// SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc_Nuked,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_YM2612[] =
{
#ifdef EC_YM2612_GPGX
	&devDef_MAME,
#endif
#ifdef EC_YM2612_GENS
	&devDef_Gens,
#endif
#ifdef EC_YM2612_NUKED
	&devDef_Nuked,
#endif
	NULL
};


#ifdef EC_YM2612_GENS
static void ym2612_gens_update(void* chip, UINT32 samples, DEV_SMPL** outputs)
{
	ym2612_* YM2612 = (ym2612_*)chip;
	
	YM2612_ClearBuffer(outputs, samples);
	YM2612_Update(YM2612, outputs, samples);
	YM2612_DacAndTimers_Update(YM2612, outputs, samples);
	
	return;
}
#endif


#ifdef EC_YM2612_GPGX
static UINT8 device_start_ym2612_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 2 / 72;
	//if (PseudoSt & 0x02))
	//	rate *= 2;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym2612_init(NULL, cfg->clock, rate, NULL, NULL);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_YM2612_GENS
static UINT8 device_start_ym2612_gens(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	ym2612_* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 2 / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = YM2612_Init(cfg->clock, rate, 0);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Gens);
	return 0x00;
}
#endif

#ifdef EC_YM2612_NUKED
static UINT8 device_start_ym2612_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 2 / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = nukedopn2_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	nukedopn2_set_mute_mask(chip, 0x00);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Nuked);
	return 0x00;
}
#endif
