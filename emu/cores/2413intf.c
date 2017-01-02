#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "2413intf.h"
#ifdef EC_YM2413_MAME
#include "ym2413.h"
#endif
#ifdef EC_YM2413_EMU2413
#include "emu2413.h"
#endif


static UINT8 device_start_ym2413_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ym2413_emu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void ym2413_set_panning_emu(void* chipptr, INT16* PanVals);



#ifdef EC_YM2413_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym2413_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym2413_read},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"YM2413", "MAME", FCC_MAME,
	
	device_start_ym2413_mame,
	ym2413_shutdown,
	ym2413_reset_chip,
	ym2413_update_one,
	
	NULL,	// SetOptionBits
	ym2413_set_mutemask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_YM2413_EMU2413
static DEVDEF_RWFUNC devFunc_Emu[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, OPLL_writeIO},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D8, 0, OPLL_writeReg},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Emu =
{
	"YM2413", "EMU2413", FCC_EMU_,
	
	device_start_ym2413_emu,
	(DEVFUNC_CTRL)OPLL_delete,
	(DEVFUNC_CTRL)OPLL_reset,
	(DEVFUNC_UPDATE)OPLL_calc_stereo,
	
	NULL,	// SetOptionBits
	(DEVFUNC_OPTMASK)OPLL_SetMuteMask,
	ym2413_set_panning_emu,
	NULL,	// SetSampleRateChangeCallback
	
	devFunc_Emu,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_YM2413[] =
{
#ifdef EC_YM2413_EMU2413
	&devDef_Emu,	// default, because it's better than MAME
#endif
#ifdef EC_YM2413_MAME
	&devDef_MAME,
#endif
	NULL
};


#ifdef EC_YM2413_MAME
static UINT8 device_start_ym2413_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT8 mode;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	mode = CHPCLK_FLAG(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = ym2413_init(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	ym2413_set_chip_mode(chip, mode);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_YM2413_EMU2413
static UINT8 device_start_ym2413_emu(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	OPLL* chip;
	UINT8 mode;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	mode = CHPCLK_FLAG(cfg->clock);
	
	rate = clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = OPLL_new(clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	OPLL_SetChipMode(chip, mode);
	
	chip->chipInf = chip;
	INIT_DEVINF(retDevInf, (DEV_DATA*)chip, rate, &devDef_Emu);
	return 0x00;
}

static void ym2413_set_panning_emu(void* chipptr, INT16* PanVals)
{
	UINT8 curChn;
	
	for (curChn = 0; curChn < 14; curChn ++)
		OPLL_set_pan((OPLL*)chipptr, curChn, PanVals[curChn]);
	
	return;
}
#endif
