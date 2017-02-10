#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "rf5cintf.h"
#ifdef EC_RF5C68_MAME
#include "rf5c68.h"
#endif
#ifdef EC_RF5C68_GENS
#include "scd_pcm.h"
#endif


static UINT8 device_start_rf5c68_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_rf5c68_gens(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_RF5C68_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, rf5c68_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, rf5c68_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, rf5c68_mem_w},
	{RWF_MEMORY | RWF_READ, DEVRW_A16D8, 0, rf5c68_mem_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, rf5c68_write_ram},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"RF5C68", "MAME", FCC_MAME,
	
	device_start_rf5c68_mame,
	device_stop_rf5c68,
	device_reset_rf5c68,
	rf5c68_update,
	
	NULL,	// SetOptionBits
	rf5c68_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_RF5C68_GENS
static DEVDEF_RWFUNC devFunc_Gens[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, SCD_PCM_Write_Reg},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, SCD_PCM_Read_Reg},
	{RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, SCD_PCM_MemWrite},
	{RWF_MEMORY | RWF_READ, DEVRW_A16D8, 0, SCD_PCM_MemRead},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, SCD_PCM_MemBlockWrite},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Gens =
{
	"RF5C164", "Gens", FCC_GENS,
	
	device_start_rf5c68_gens,
	SCD_PCM_Deinit,
	SCD_PCM_Reset,
	SCD_PCM_Update,
	
	NULL,	// SetOptionBits
	SCD_PCM_SetMuteMask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_Gens,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_RF5C68[] =
{
#ifdef EC_RF5C68_MAME
	&devDef_MAME,
#endif
#ifdef EC_RF5C68_GENS
	&devDef_Gens,
#endif
	NULL
};


#ifdef EC_RF5C68_MAME
static UINT8 device_start_rf5c68_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 384;
	chip = device_start_rf5c68(cfg->clock);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_RF5C68_GENS
static UINT8 device_start_rf5c68_gens(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 384;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = SCD_PCM_Init(cfg->clock, rate, cfg->flags);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Gens);
	return 0x00;
}
#endif
