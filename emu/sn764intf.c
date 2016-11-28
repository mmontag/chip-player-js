#include <stddef.h>
#include <stdlib.h>

#include <stdtype.h>
#include "EmuStructs.h"

#include "sn764intf.h"
#ifdef EC_MAME
#include "sn76496.h"
#endif
#ifdef EC_MAXIM
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

#ifdef EC_MAME
DEVINF_RWFUNC devFunc_MAME[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, sn76496_w_mame},
};
DEV_INFO devInf_MAME =
{
	NULL, 0,
	
	(DEVFUNC_START)device_start_sn76496_mame,
	(DEVFUNC_CTRL)device_stop_sn76496_mame,
	sn76496_reset,
	SN76496Update,
	
	NULL,	// SetOptionBits
	sn76496_set_mutemask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangCallback
	
	1, devFunc_MAME,	// rwFuncs
};
#endif
#ifdef EC_MAXIM
DEVINF_RWFUNC devFunc_Maxim[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, sn76496_w_maxim},
};
DEV_INFO devInf_Maxim =
{
	NULL, 0,
	
	(DEVFUNC_START)device_start_sn76496_maxim,
	(DEVFUNC_CTRL)device_stop_sn76496_maxim,
	(DEVFUNC_CTRL)SN76489_Reset,
	(DEVFUNC_UPDATE)SN76489_Update,
	
	NULL,	// SetOptionBits
	sn76489_mute_maxim,
	sn76489_pan_maxim,
	NULL,	// SetSampleRateChangCallback
	
	1, devFunc_Maxim,	// rwFuncs
};
#endif

DEVINF_LIST devInfList_SN76496[] =
{
#ifdef EC_MAME
	{&devInf_MAME, 0x00},
#endif
#ifdef EC_MAXIM
	{&devInf_Maxim, 0x01},
#endif
	{NULL, 0x00}
};


#ifdef EC_MAME
static UINT8 device_start_sn76496_mame(const SN76496_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	SN76496_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = sn76496_start(&chip, cfg->_genCfg.clock, cfg->shiftRegWidth, cfg->noiseTaps,
						cfg->negate, cfg->stereo, cfg->clkDiv, ! cfg->segaPSG);
	if (! rate)
		return 0xFF;
	
	info = (SN76496_INF*)malloc(sizeof(SN76496_INF));
	info->chip.mame = chip;
	info->cfg = *cfg;
	sn76496_freq_limiter(info->chip.mame, cfg->_genCfg.smplRate);
	
	devData = (DEV_DATA*)info->chip.mame;
	devData->chipInf = info;	// store pointer to SN76496_INF into sound chip structure
	
	*retDevInf = devInf_MAME;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	return 0x00;
}
#endif

#ifdef EC_MAXIM
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
	SN76489_Config(info->chip.maxim, cfg->noiseTaps, cfg->shiftRegWidth, 0);
	
	devData = (DEV_DATA*)info->chip.any;
	devData->chipInf = info;	// store pointer to SN76496_INF into sound chip structure
	
	*retDevInf = devInf_Maxim;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	return 0x00;
}
#endif

#ifdef EC_MAME
static void device_stop_sn76496_mame(DEV_DATA* chipptr)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	sn76496_shutdown(info->chip.mame);
	free(info);
	return;
}
#endif

#ifdef EC_MAXIM
static void device_stop_sn76496_maxim(DEV_DATA* chipptr)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	SN76489_Shutdown(info->chip.maxim);
	free(info);
	return;
}
#endif


#ifdef EC_MAME
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

#ifdef EC_MAXIM
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
