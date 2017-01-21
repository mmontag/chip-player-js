#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "c6280intf.h"
#ifdef EC_C6280_MAME
#include "c6280_mame.h"
#endif
#ifdef EC_C6280_OOTAKE
#include "Ootake_PSG.h"
#endif


static UINT8 device_start_c6280_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_c6280_ootake(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);


#ifdef EC_C6280_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, c6280mame_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, c6280mame_r},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"HuC6280", "MAME", FCC_MAME,
	
	device_start_c6280_mame,
	device_stop_c6280mame,
	device_reset_c6280mame,
	c6280mame_update,
	
	NULL,	// SetOptionBits
	c6280mame_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_C6280_OOTAKE
static DEVDEF_RWFUNC devFunc_Ootake[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, OotakePSG_Write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, OotakePSG_Read},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Ootake =
{
	"HuC6280", "Ootake", FCC_OOTK,
	
	device_start_c6280_ootake,
	OotakePSG_Deinit,
	OotakePSG_ResetVolumeReg,	// TODO: not a "real" reset
	OotakePSG_Mix,
	
	NULL,	// SetOptionBits
	OotakePSG_SetMuteMask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_Ootake,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_C6280[] =
{
#ifdef EC_C6280_OOTAKE
	&devDef_Ootake,
#endif
#ifdef EC_C6280_MAME
	&devDef_MAME,
#endif
	NULL
};


#ifdef EC_C6280_MAME
static UINT8 device_start_c6280_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	
	rate = clock / 16;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = device_start_c6280mame(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_C6280_OOTAKE
static UINT8 device_start_c6280_ootake(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT8 mode;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	mode = CHPCLK_FLAG(cfg->clock);
	
	rate = clock / 16;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = OotakePSG_Init(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	OotakePSG_SetHoneyInTheSky(chip, mode);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Ootake);
	return 0x00;
}
#endif
