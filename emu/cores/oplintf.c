#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"

#include "oplintf.h"
#ifdef EC_YM3812_MAME
#include "fmopl.h"
#endif
#ifdef EC_YM3812_ADLIBEMU
#define OPLTYPE_IS_OPL2
#include "adlibemu.h"
#endif


static UINT8 device_start_ym3812_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym3812_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym3526(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_y8950(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_YM3812_MAME
static DEVDEF_RWFUNC devFunc3812_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym3812_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym3812_read},
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
	
	devFunc3812_MAME,	// rwFuncs
};
#endif
#ifdef EC_YM3812_ADLIBEMU
static DEVDEF_RWFUNC devFunc3812_Emu[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, adlib_OPL2_writeIO},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, adlib_OPL2_reg_read},
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
	
	devFunc3812_Emu,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_YM3812[] =
{
#ifdef EC_YM3812_ADLIBEMU
	&devDef3812_AdLibEmu,	// default, because it's better than MAME
#endif
#ifdef EC_YM3812_MAME
	&devDef3812_MAME,
#endif
	NULL
};


static DEVDEF_RWFUNC devFunc3526_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym3526_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym3526_read},
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
	
	devFunc3526_MAME,	// rwFuncs
};

const DEV_DEF* devDefList_YM3526[] =
{
	&devDef3526_MAME,
	NULL
};


static DEVDEF_RWFUNC devFunc8950_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, y8950_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, y8950_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, y8950_write_pcmrom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, y8950_alloc_pcmrom},
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
	
	devFunc8950_MAME,	// rwFuncs
};

const DEV_DEF* devDefList_Y8950[] =
{
	&devDef8950_MAME,
	NULL
};


#ifdef EC_YM3812_MAME
static UINT8 device_start_ym3812_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym3812_init(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// YM3812 setup
	//ym3812_set_timer_handler (chip, TimerHandler, chip);
	//ym3812_set_irq_handler   (chip, IRQHandler, chip);
	//ym3812_set_update_handler(chip, stream_update3812_mame, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	retDevInf->devDef = &devDef3812_MAME;
	return 0x00;
}
#endif

#ifdef EC_YM3812_ADLIBEMU
static UINT8 device_start_ym3812_adlibemu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = adlib_OPL2_init(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	retDevInf->devDef = &devDef3812_AdLibEmu;
	return 0x00;
}
#endif


static UINT8 device_start_ym3526(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym3526_init(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	// YM3526 setup
	//ym3526_set_timer_handler (chip, TimerHandler, chip);
	//ym3526_set_irq_handler   (chip, IRQHandler, chip);
	//ym3526_set_update_handler(chip, stream_update3526, chip);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	retDevInf->devDef = &devDef3526_MAME;
	return 0x00;
}


static UINT8 device_start_y8950(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = y8950_init(clock, rate);
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
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	retDevInf->devDef = &devDef8950_MAME;
	return 0x00;
}
