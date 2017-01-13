#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "ayintf.h"
#ifdef EC_AY8910_MAME
#include "ay8910.h"
#endif
#ifdef EC_AY8910_EMU2149
#include "emu2149.h"
#endif


static UINT8 device_start_ay8910_mame(const AY8910_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_ay8910_emu(const AY8910_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_AY8910_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ay8910_write},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D8, 0, ay8910_write_reg},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ay8910_read},
	{RWF_CLOCK | RWF_WRITE, DEVRW_VALUE, 0, ay8910_set_clock},
	{RWF_SRATE | RWF_READ, DEVRW_VALUE, 0, ay8910_get_sample_rate},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"AY8910", "MAME", FCC_MAME,
	
	(DEVFUNC_START)device_start_ay8910_mame,
	ay8910_stop,
	ay8910_reset,
	ay8910_update_one,
	
	NULL,	// SetOptionBits
	ay8910_set_mute_mask,
	NULL,	// SetPanning
	ay8910_set_srchg_cb,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_AY8910_EMU2149
static DEVDEF_RWFUNC devFunc_Emu[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, PSG_writeIO},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, PSG_readIO},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D8, 0, PSG_writeReg},
	{RWF_REGISTER | RWF_QUICKREAD, DEVRW_A8D8, 0, PSG_readReg},
	{RWF_CLOCK | RWF_WRITE, DEVRW_VALUE, 0, PSG_set_clock},
	{RWF_SRATE | RWF_WRITE, DEVRW_VALUE, 0, PSG_set_rate},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Emu =
{
	"YM2149", "EMU2149", FCC_EMU_,
	
	(DEVFUNC_START)device_start_ay8910_emu,
	(DEVFUNC_CTRL)PSG_delete,
	(DEVFUNC_CTRL)PSG_reset,
	(DEVFUNC_UPDATE)PSG_calc_stereo,
	
	NULL,	// SetOptionBits
	(DEVFUNC_OPTMASK)PSG_setMask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_Emu,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_AY8910[] =
{
#ifdef EC_AY8910_EMU2149
	&devDef_Emu,
#endif
#ifdef EC_AY8910_MAME
	&devDef_MAME,
#endif
	NULL
};


#ifdef EC_AY8910_MAME
static UINT8 device_start_ay8910_mame(const AY8910_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->_genCfg.clock);
	
	rate = ay8910_start(&chip, clock, cfg->chipType, cfg->chipFlags);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_AY8910_EMU2149
static UINT8 device_start_ay8910_emu(const AY8910_CFG* cfg, DEV_INFO* retDevInf)
{
	PSG* chip;
	DEV_DATA* devData;
	UINT8 isYM;
	UINT8 flags;
	UINT32 clock;
	UINT32 rate;
	
	clock = CHPCLK_CLOCK(cfg->_genCfg.clock);
	isYM = ((cfg->chipType & 0xF0) > 0x00);
	flags = cfg->chipFlags;
	if (! isYM)
		flags &= ~YM2149_PIN26_LOW;
	
	if (flags & YM2149_PIN26_LOW)
		rate = clock / 2 / 8;
	else
		rate = clock / 8;
	SRATE_CUSTOM_HIGHEST(cfg->_genCfg.srMode, rate, cfg->_genCfg.smplRate);
	
	chip = PSG_new(clock, rate);
	if (chip == NULL)
		return 0xFF;
	PSG_setVolumeMode(chip, isYM ? 1 : 2);
	PSG_setFlags(chip, flags);
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Emu);
	return 0x00;
}
#endif
