#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "saaintf.h"
#ifdef EC_SAA1099_MAME
#include "saa1099_mame.h"
#endif
#ifdef EC_SAA1099_NRS
//#include "saa1099_nrs.h"
#endif


static UINT8 device_start_saa1099_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_saa1099_nrs(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);



#ifdef EC_SAA1099_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, saa1099m_write},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"SAA1099", "MAME", FCC_MAME,
	
	device_start_saa1099_mame,
	saa1099m_destroy,
	saa1099m_reset,
	saa1099m_update,
	
	NULL,	// SetOptionBits
	saa1099m_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_SAA1099_NRS
static DEVDEF_RWFUNC devFunc_NRS[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, saa1099m_write},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_NRS =
{
	"SAA1099", "NewRisingSun", FCC_NRS_,
	
	device_start_saa1099_nrs,
	saa1099m_destroy,
	saa1099m_reset,
	saa1099m_update,
	
	NULL,	// SetOptionBits
	saa1099m_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_NRS,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_SAA1099[] =
{
#ifdef EC_SAA1099_MAME
	&devDef_MAME,
#endif
#ifdef EC_SAA1099_NRS
	&devDef_NRS,
#endif
	NULL
};


#ifdef EC_SAA1099_MAME
static UINT8 device_start_saa1099_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 128;	// /128 seems right based on the highest noise frequency
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = saa1099m_create(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_SAA1099_NRS
static UINT8 device_start_saa1099_nrs(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 128;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = NULL;
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_NRS);
	return 0x00;
}
#endif
