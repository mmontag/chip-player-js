#include <stdlib.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "262intf.h"
#ifdef EC_YMF262_MAME
#include "ymf262.h"
#endif
#ifdef EC_YMF262_ADLIBEMU
#define OPLTYPE_IS_OPL3
#include "adlibemu.h"
#endif
#ifdef EC_YMF262_NUKED
#include "nukedopl3.h"
#endif


static UINT8 device_start_ymf262_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ymf262_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ymf262_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_YMF262_MAME
static DEVDEF_RWFUNC devFunc262_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ymf262_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ymf262_read},
	{RWF_VOLUME | RWF_WRITE, DEVRW_VALUE, 0, ymf262_set_volume},
	{RWF_VOLUME_LR | RWF_WRITE, DEVRW_VALUE, 0, ymf262_set_vol_lr},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ymf262_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef262_MAME =
{
	"YMF262", "MAME", FCC_MAME,
	
	device_start_ymf262_mame,
	ymf262_shutdown,
	ymf262_reset_chip,
	ymf262_update_one,
	
	NULL,	// SetOptionBits
	ymf262_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	ymf262_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc262_MAME,	// rwFuncs
};
#endif
#ifdef EC_YMF262_ADLIBEMU
static DEVDEF_RWFUNC devFunc262_Emu[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, adlib_OPL3_writeIO},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, adlib_OPL3_reg_read},
	{RWF_VOLUME | RWF_WRITE, DEVRW_VALUE, 0, adlib_OPL3_set_volume},
	{RWF_VOLUME_LR | RWF_WRITE, DEVRW_VALUE, 0, adlib_OPL3_set_volume_lr},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, adlib_OPL3_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef262_AdLibEmu =
{
	"YMF262", "AdLibEmu", FCC_ADLE,
	
	device_start_ymf262_adlibemu,
	adlib_OPL3_stop,
	adlib_OPL3_reset,
	adlib_OPL3_getsample,
	
	NULL,	// SetOptionBits
	adlib_OPL3_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc262_Emu,	// rwFuncs
};
#endif
#ifdef EC_YMF262_NUKED
static DEVDEF_RWFUNC devFunc262_Nuked[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nukedopl3_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, nukedopl3_read},
	{RWF_VOLUME | RWF_WRITE, DEVRW_VALUE, 0, nukedopl3_set_volume},
	{RWF_VOLUME_LR | RWF_WRITE, DEVRW_VALUE, 0, nukedopl3_set_vol_lr},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, nukedopl3_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef262_Nuked =
{
	"YMF262", "Nuked OPL3", FCC_NUKE,
	
	device_start_ymf262_nuked,
	nukedopl3_shutdown,
	nukedopl3_reset_chip,
	nukedopl3_update,
	
	NULL,	// SetOptionBits
	nukedopl3_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc262_Nuked,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_YMF262[] =
{
#ifdef EC_YMF262_ADLIBEMU
	&devDef262_AdLibEmu,	// default, because it's better than MAME
#endif
#ifdef EC_YMF262_MAME
	&devDef262_MAME,
#endif
#ifdef EC_YMF262_NUKED
	&devDef262_Nuked,
#endif
	NULL
};


#ifdef EC_YMF262_MAME
static UINT8 device_start_ymf262_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 288;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ymf262_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// YMF262 setup
	//ymf262_set_timer_handler (chip, TimerHandler, chip);
	//ymf262_set_irq_handler   (chip, IRQHandler, chip);
	//ymf262_set_update_handler(chip, stream_update262_mame, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef262_MAME);
	return 0x00;
}
#endif

#ifdef EC_YMF262_ADLIBEMU
static UINT8 device_start_ymf262_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 288;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = adlib_OPL3_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef262_AdLibEmu);
	return 0x00;
}
#endif

#ifdef EC_YMF262_NUKED
static UINT8 device_start_ymf262_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 288;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = nukedopl3_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	nukedopl3_set_volume(chip, 0x10000);
	nukedopl3_set_mute_mask(chip, 0x000000);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef262_Nuked);
	return 0x00;
}
#endif
