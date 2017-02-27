#include <stdlib.h>

#include <stdtype.h>
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
#include "nukedopl.h"
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
	ymf262_set_mutemask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
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
	NULL,	// LinkDevice
	
	devFunc262_Emu,	// rwFuncs
};
#endif
#ifdef EC_YMF262_NUKED
static void nuked_write(void *chip, UINT8 a, UINT8 v)
{
	static UINT16 address = 0;
	opl3_chip* opl3 = (opl3_chip*) chip;

	switch(a&3)
	{
	case 0:
		address = v;
		break;
	case 1:
	case 3:
		OPL3_WriteRegBuffered(opl3, address, v);
		break;
	case 2:
		address = v | 0x100;
		break;
	}
}
static void nuked_shutdown(void *chip)
{
	free(chip);
}
static void nuked_reset_chip(void *chip)
{
	opl3_chip* opl3 = (opl3_chip*) chip;
	UINT32 rate;

	rate = opl3->smplRate;
	OPL3_Reset(opl3, rate);
	opl3->smplRate = rate; // save for reset
}
static void nuked_update(void *chip, UINT32 samples, DEV_SMPL **out)
{
	opl3_chip* opl3 = (opl3_chip*) chip;
	DEV_SMPL *bufMO = out[0];
	DEV_SMPL *bufRO = out[1];
	DEV_SMPL buffers[2];
	UINT32 i;

	for( i=0; i < samples ; i++ )
	{
		OPL3_GenerateResampled(opl3, buffers);
		bufMO[i] = buffers[0];
		bufRO[i] = buffers[1];
	}
}
static DEVDEF_RWFUNC devFunc262_Nuked[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, nuked_write},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef262_Nuked =
{
	"YMF262", "Nuked OPL3", FCC_NUKE,
	
	device_start_ymf262_nuked,
	nuked_shutdown,
	nuked_reset_chip,
	nuked_update,
	
	NULL,	// SetOptionBits
	NULL,   // SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
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
	opl3_chip*	opl3;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 288;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	opl3 = (opl3_chip3*) malloc(sizeof(opl3_chip));
	if (opl3 == NULL)
		return 0xFF;

	opl3->smplRate = rate; // save for reset
	
	devData = (DEV_DATA*) opl3;
	devData->chipInf = (void*) opl3;
	INIT_DEVINF(retDevInf, devData, rate, &devDef262_Nuked);
	return 0x00;
}
#endif