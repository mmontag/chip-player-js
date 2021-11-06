#include <stdlib.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#ifndef SNDDEV_SELECT
// if not asked to select certain sound devices, just include everything (comfort option)
#define SNDDEV_YM3812
#define SNDDEV_YM3526
#define SNDDEV_Y8950
#endif

#include "oplintf.h"
#if defined(EC_YM3812_MAME) || defined(SNDDEV_YM3526) || defined(SNDDEV_Y8950)
#include "fmopl.h"
#endif
#ifdef EC_YM3812_ADLIBEMU
#define OPLTYPE_IS_OPL2
#include "adlibemu.h"
#endif
#ifdef EC_YM3812_NUKED
#include "nukedopl3.h"
#endif


static UINT8 device_start_ym3812_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym3812_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym3526(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_y8950(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym3812_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_YM3812_MAME
static DEVDEF_RWFUNC devFunc3812_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym3812_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym3812_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, opl_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef3812_MAME =
{
	"YM3812", "MAME", FCC_MAME,
	
	device_start_ym3812_mame,
	ym3812_shutdown,
	ym3812_reset_chip,
	ym3812_update_one,
	
	NULL,	// SetOptionBits
	opl_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	opl_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc3812_MAME,	// rwFuncs
};
#endif	// EC_YM3812_MAME
#ifdef EC_YM3812_ADLIBEMU
static DEVDEF_RWFUNC devFunc3812_Emu[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, adlib_OPL2_writeIO},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, adlib_OPL2_reg_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, adlib_OPL2_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef3812_AdLibEmu =
{
	"YM3812", "AdLibEmu", FCC_ADLE,
	
	device_start_ym3812_adlibemu,
	adlib_OPL2_stop,
	adlib_OPL2_reset,
	adlib_OPL2_getsample,
	
	NULL,	// SetOptionBits
	adlib_OPL2_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc3812_Emu,	// rwFuncs
};
#endif	// EC_YM3812_ADLIBEMU
#ifdef EC_YM3812_NUKED
static DEVDEF_RWFUNC devFunc3812_Nuked[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nukedopl3_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, nukedopl3_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, nukedopl3_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef3812_Nuked =
{
	"YM3812", "Nuked OPL3", FCC_NUKE,
	
	device_start_ym3812_nuked,
	nukedopl3_shutdown,
	nukedopl3_reset_chip,
	nukedopl3_update,
	
	NULL,	// SetOptionBits
	nukedopl3_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc3812_Nuked,	// rwFuncs
};
#endif	// EC_YM3812_NUKED

#ifdef SNDDEV_YM3812
const DEV_DEF* devDefList_YM3812[] =
{
#ifdef EC_YM3812_ADLIBEMU
	&devDef3812_AdLibEmu,	// default, because it's better than MAME
#endif
#ifdef EC_YM3812_MAME
	&devDef3812_MAME,
#endif
#ifdef EC_YM3812_NUKED
	&devDef3812_Nuked,	// note: OPL3 emulator, so some things aren't working (OPL1 mode, OPL2 detection)
#endif
	NULL
};
#endif


#ifdef SNDDEV_YM3526
static DEVDEF_RWFUNC devFunc3526_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym3526_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym3526_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, opl_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef3526_MAME =
{
	"YM3526", "MAME", FCC_MAME,
	
	device_start_ym3526,
	ym3526_shutdown,
	ym3526_reset_chip,
	ym3526_update_one,
	
	NULL,	// SetOptionBits
	opl_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	opl_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc3526_MAME,	// rwFuncs
};

const DEV_DEF* devDefList_YM3526[] =
{
	&devDef3526_MAME,
	NULL
};
#endif	// SNDDEV_YM3526


#ifdef SNDDEV_Y8950
static DEVDEF_RWFUNC devFunc8950_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, y8950_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, y8950_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, y8950_write_pcmrom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, y8950_alloc_pcmrom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, opl_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef8950_MAME =
{
	"Y8950", "MAME", FCC_MAME,
	
	device_start_y8950,
	y8950_shutdown,
	y8950_reset_chip,
	y8950_update_one,
	
	NULL,	// SetOptionBits
	opl_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	opl_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc8950_MAME,	// rwFuncs
};

const DEV_DEF* devDefList_Y8950[] =
{
	&devDef8950_MAME,
	NULL
};
#endif	// SNDDEV_Y8950


#ifdef EC_YM3812_MAME
static UINT8 device_start_ym3812_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym3812_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// YM3812 setup
	//ym3812_set_timer_handler (chip, TimerHandler, chip);
	//ym3812_set_irq_handler   (chip, IRQHandler, chip);
	//ym3812_set_update_handler(chip, stream_update3812_mame, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef3812_MAME);
	return 0x00;
}
#endif	// EC_YM3812_MAME

#ifdef EC_YM3812_ADLIBEMU
static UINT8 device_start_ym3812_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = adlib_OPL2_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef3812_AdLibEmu);
	return 0x00;
}
#endif	// EC_YM3812_ADLIBEMU

#ifdef EC_YM3812_NUKED
static UINT8 device_start_ym3812_nuked(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = nukedopl3_init(cfg->clock * 4, rate);
	if (chip == NULL)
		return 0xFF;
	
	nukedopl3_set_volume(chip, 0x10000);
	nukedopl3_set_mute_mask(chip, 0x000000);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef3812_Nuked);
	return 0x00;
}
#endif	// EC_YM3812_NUKED


#ifdef SNDDEV_YM3526
static UINT8 device_start_ym3526(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym3526_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// YM3526 setup
	//ym3526_set_timer_handler (chip, TimerHandler, chip);
	//ym3526_set_irq_handler   (chip, IRQHandler, chip);
	//ym3526_set_update_handler(chip, stream_update3526, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef3526_MAME);
	return 0x00;
}
#endif	// SNDDEV_YM3526


#ifdef SNDDEV_Y8950
static UINT8 device_start_y8950(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = y8950_init(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// port and keyboard handler
	//y8950_set_port_handler(chip, Y8950PortHandler_w, Y8950PortHandler_r, chip);
	//y8950_set_keyboard_handler(chip, Y8950KeyboardHandler_w, Y8950KeyboardHandler_r, chip);

	// YM8950 setup
	//y8950_set_timer_handler (chip, TimerHandler, chip);
	//y8950_set_irq_handler   (chip, IRQHandler, chip);
	//y8950_set_update_handler(chip, stream_update8950, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef8950_MAME);
	return 0x00;
}
#endif	// SNDDEV_Y8950
