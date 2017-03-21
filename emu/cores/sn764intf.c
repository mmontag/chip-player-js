#include <stdlib.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"

#include "sn764intf.h"
#ifdef EC_SN76496_MAME
#include "sn76496.h"
#endif
#ifdef EC_SN76496_MAXIM
#include "sn76489.h"
#endif


static UINT8 device_start_sn76496_mame(const SN76496_CFG* cfg, DEV_INFO* retDevInf);
static UINT8 device_start_sn76496_maxim(const SN76496_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_sn76496_mame(DEV_DATA* chipptr);
static void device_stop_sn76496_maxim(DEV_DATA* chipptr);
static void sn76496_w_mame(DEV_DATA* chipptr, UINT8 reg, UINT8 data);
static void sn76496_w_maxim(DEV_DATA* chipptr, UINT8 reg, UINT8 data);
static void sn76489_mute_maxim(void* chipptr, UINT32 MuteMask);
static void sn76489_pan_maxim(void* chipptr, INT16* PanVals);


typedef struct _sn76496_info
{
	union
	{
		void* any;
		void* mame;
		SN76489_Context* maxim;
	} chip;
	SN76496_CFG cfg;
} SN76496_INF;

#ifdef EC_SN76496_MAME
static DEVDEF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, sn76496_w_mame},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME =
{
	"SN76496", "MAME", FCC_MAME,
	
	(DEVFUNC_START)device_start_sn76496_mame,
	(DEVFUNC_CTRL)device_stop_sn76496_mame,
	sn76496_reset,
	SN76496Update,
	
	NULL,	// SetOptionBits
	sn76496_set_mutemask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_SN76496_MAXIM
static DEVDEF_RWFUNC devFunc_Maxim[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, sn76496_w_maxim},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_Maxim =
{
	"SN76489", "Maxim", FCC_MAXM,
	
	(DEVFUNC_START)device_start_sn76496_maxim,
	(DEVFUNC_CTRL)device_stop_sn76496_maxim,
	(DEVFUNC_CTRL)SN76489_Reset,
	(DEVFUNC_UPDATE)SN76489_Update,
	
	NULL,	// SetOptionBits
	sn76489_mute_maxim,
	sn76489_pan_maxim,
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc_Maxim,	// rwFuncs
};
#endif

const DEV_DEF* devDefList_SN76496[] =
{
#ifdef EC_SN76496_MAME
	&devDef_MAME,
#endif
#ifdef EC_SN76496_MAXIM
	&devDef_Maxim,
#endif
	NULL
};


#ifdef EC_SN76496_MAME
static UINT8 device_start_sn76496_mame(const SN76496_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	SN76496_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = sn76496_start(&chip, cfg->_genCfg.clock, cfg->shiftRegWidth, cfg->noiseTaps,
						cfg->negate, cfg->stereo, cfg->clkDiv, cfg->segaPSG);
	if (! rate)
		return 0xFF;
	
	info = (SN76496_INF*)malloc(sizeof(SN76496_INF));
	info->chip.mame = chip;
	info->cfg = *cfg;
	
	if (cfg->t6w28_tone != NULL)
		sn76496_connect_t6w28(info->chip.mame, cfg->t6w28_tone);
	sn76496_freq_limiter(info->chip.mame, cfg->_genCfg.smplRate);
	
	devData = (DEV_DATA*)info->chip.mame;
	devData->chipInf = info;	// store pointer to SN76496_INF into sound chip structure
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME);
	return 0x00;
}
#endif

#ifdef EC_SN76496_MAXIM
static UINT8 device_start_sn76496_maxim(const SN76496_CFG* cfg, DEV_INFO* retDevInf)
{
	SN76489_Context* chip;
	SN76496_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->_genCfg.smplRate;
	chip = SN76489_Init(cfg->_genCfg.clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	info = (SN76496_INF*)malloc(sizeof(SN76496_INF));
	info->chip.maxim = chip;
	info->cfg = *cfg;
	
	if (cfg->t6w28_tone != NULL)
		SN76489_ConnectT6W28(info->chip.maxim, (SN76489_Context*)cfg->t6w28_tone);
	SN76489_Config(info->chip.maxim, cfg->noiseTaps, cfg->shiftRegWidth);
	
	devData = &info->chip.maxim->_devData;
	devData->chipInf = info;	// store pointer to SN76496_INF into sound chip structure
	INIT_DEVINF(retDevInf, devData, rate, &devDef_Maxim);
	return 0x00;
}
#endif

#ifdef EC_SN76496_MAME
static void device_stop_sn76496_mame(DEV_DATA* chipptr)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	sn76496_shutdown(info->chip.mame);
	free(info);
	return;
}
#endif

#ifdef EC_SN76496_MAXIM
static void device_stop_sn76496_maxim(DEV_DATA* chipptr)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	SN76489_Shutdown(info->chip.maxim);
	free(info);
	return;
}
#endif


#ifdef EC_SN76496_MAME
static void sn76496_w_mame(DEV_DATA* chipptr, UINT8 reg, UINT8 data)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	switch(reg)
	{
	case 0x00:
		sn76496_write_reg(info->chip.mame, 0x00, data);
		break;
	case 0x01:
		sn76496_stereo_w(info->chip.mame, 0x00, data);
		break;
	}
	return;
}
#endif

#ifdef EC_SN76496_MAXIM
static void sn76496_w_maxim(DEV_DATA* chipptr, UINT8 reg, UINT8 data)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	switch(reg)
	{
	case 0x00:
		SN76489_Write(info->chip.maxim, data);
		break;
	case 0x01:
		SN76489_GGStereoWrite(info->chip.maxim, data);
		break;
	}
	return;
}

static void sn76489_mute_maxim(void* chipptr, UINT32 MuteMask)
{
	SN76489_SetMute((SN76489_Context*)chipptr, ~MuteMask & 0x0F);
	return;
}

static void sn76489_pan_maxim(void* chipptr, INT16* PanVals)
{
	SN76489_SetPanning((SN76489_Context*)chipptr, PanVals[0x00], PanVals[0x01], PanVals[0x02], PanVals[0x03]);
	return;
}
#endif
