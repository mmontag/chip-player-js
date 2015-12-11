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



//UINT8 device_start_sn76496(const SN76496_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_sn76496(DEV_DATA* chipptr);
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
	DEVFUNC_CTRL reset;
} SN76496_INF;

#ifdef EC_MAME
DEV_INFO devInf_MAME =
{
	NULL, 0,
	
	(DEVFUNC_CTRL)device_stop_sn76496,
	sn76496_reset,
	SN76496Update,
	
	NULL,	// SetOptionBits
	sn76496_set_mutemask,
	NULL,	// SetPanning
	
	{
		DEVRW_A8D8, DEVRW_A8D8, 0x00,
		NULL, sn76496_w_mame, NULL
	}
};
#endif
#ifdef EC_MAXIM
DEV_INFO devInf_Maxim =
{
	NULL, 0,
	
	(DEVFUNC_CTRL)device_stop_sn76496,
	(DEVFUNC_CTRL)SN76489_Reset,
	(DEVFUNC_UPDATE)SN76489_Update,
	
	NULL,	// SetOptionBits
	sn76489_mute_maxim,
	sn76489_pan_maxim,
	
	{
		DEVRW_A8D8, DEVRW_A8D8, 0x00,
		NULL, sn76496_w_maxim, NULL
	}
};
#endif


UINT8 device_start_sn76496(const SN76496_CFG* cfg, DEV_INFO* retDevInf)
{
	SN76496_INF* info;
	DEV_INFO* devIRef;
	DEV_DATA* devData;
	UINT32 rate;
	
	devIRef = NULL;
	rate = 0;
	info = (SN76496_INF*)malloc(sizeof(SN76496_INF));
	info->cfg = *cfg;
	switch(cfg->_genCfg.emuCore)
	{
#ifdef EC_MAME
	case EC_MAME:
		rate = sn76496_start(&info->chip.mame, cfg->_genCfg.clock, cfg->shiftRegWidth, cfg->noiseTaps,
							cfg->negate, cfg->stereo, cfg->clkDiv, ! cfg->segaPSG);
		if (! rate)
		{
			info->chip.mame = NULL;
			break;
		}
		info->reset = sn76496_reset;
		sn76496_freq_limiter(info->chip.mame, cfg->_genCfg.smplRate);
		
		devIRef = &devInf_MAME;
		break;
#endif
#ifdef EC_MAXIM
	case EC_MAXIM:
		rate = cfg->_genCfg.smplRate;
		info->chip.maxim = SN76489_Init(cfg->_genCfg.clock, rate);
		if (info->chip.maxim == NULL)
			break;
		info->reset = (DEVFUNC_CTRL)SN76489_Reset;
		SN76489_Config(info->chip.maxim, cfg->noiseTaps, cfg->shiftRegWidth, 0);
		
		devIRef = &devInf_Maxim;
		break;
#endif
	}
	
	if (info->chip.any == NULL)
	{
		free(info);
		return 0xFF;
	}
	devData = (DEV_DATA*)info->chip.any;
	devData->chipInf = info;	// store pointer to SN76496_INF into sound chip structure
	
	*retDevInf = *devIRef;
	retDevInf->dataPtr = devData;
	retDevInf->sampleRate = rate;
	return 0x00;
}

static void device_stop_sn76496(DEV_DATA* chipptr)
{
	SN76496_INF* info = (SN76496_INF*)chipptr->chipInf;
	switch(info->cfg._genCfg.emuCore)
	{
#ifdef EC_MAME
	case EC_MAME:
		sn76496_shutdown(info->chip.mame);
		break;
#endif
#ifdef EC_MAXIM
	case EC_MAXIM:
		SN76489_Shutdown(info->chip.maxim);
		break;
#endif
	}
	free(info);
	return;
}


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
