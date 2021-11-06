#include <stdlib.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../SoundDevs.h"
#include "../SoundEmu.h"
#include "../EmuHelper.h"

#ifndef SNDDEV_SELECT
// if not asked to select certain sound devices, just include everything (comfort option)
#define SNDDEV_YM2203
#define SNDDEV_YM2608
#define SNDDEV_YM2610
#endif

#include "opnintf.h"
#include "fmopn.h"
#include "ayintf.h"	// for constants and AY8910_CFG struct


#define LINKDEV_SSG	0x00

static UINT8 get_ssg_funcs(const DEV_DEF* devDef, ssg_callbacks* retFuncs);
static AY8910_CFG* get_ssg_config(const DEV_GEN_CFG* cfg, UINT32 clockDiv, UINT8 ssgType);
static void init_ssg_devinfo(DEV_INFO* devInf, const DEV_GEN_CFG* baseCfg, UINT8 clockDiv, UINT8 ssgType);

static UINT8 device_start_ym2203(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_ym2203(void* param);
static UINT8 device_ym2203_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG);

static UINT8 device_start_ym2608(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_ym2608(void* param);
static UINT8 device_ym2608_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG);

static UINT8 device_start_ym2610(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_ym2610(void* param);
static UINT8 device_ym2610_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG);


typedef struct _opn_info
{
	void* opn;
	void* ssg;
} OPN_INF;


#ifdef SNDDEV_YM2203
static DEVDEF_RWFUNC devFunc_MAME_2203[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym2203_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym2203_read},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ym2203_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME_2203 =
{
	"YM2203", "MAME", FCC_MAME,
	
	device_start_ym2203,
	device_stop_ym2203,
	ym2203_reset_chip,
	ym2203_update_one,
	
	NULL,	// SetOptionBits
	ym2203_set_mute_mask,	// SetMuteMask
	NULL,	// SetPanning
	ym2203_set_srchg_cb,	// SetSampleRateChangeCallback
	ym2203_set_log_cb,	// SetLoggingCallback
	device_ym2203_link_ssg,	// LinkDevice
	
	devFunc_MAME_2203,	// rwFuncs
};

const DEV_DEF* devDefList_YM2203[] =
{
	&devDef_MAME_2203,
	NULL
};
#endif	// SNDDEV_YM2203

#ifdef SNDDEV_YM2608
static DEVDEF_RWFUNC devFunc_MAME_2608[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym2608_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym2608_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'B', ym2608_write_pcmromb},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'B', ym2608_alloc_pcmromb},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ym2608_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME_2608 =
{
	"YM2608", "MAME", FCC_MAME,
	
	device_start_ym2608,
	device_stop_ym2608,
	ym2608_reset_chip,
	ym2608_update_one,
	
	NULL,	// SetOptionBits
	ym2608_set_mute_mask,	// SetMuteMask
	NULL,	// SetPanning
	ym2608_set_srchg_cb,	// SetSampleRateChangeCallback
	ym2608_set_log_cb,	// SetLoggingCallback
	device_ym2608_link_ssg,	// LinkDevice
	
	devFunc_MAME_2608,	// rwFuncs
};

const DEV_DEF* devDefList_YM2608[] =
{
	&devDef_MAME_2608,
	NULL
};
#endif	// SNDDEV_YM2608

#ifdef SNDDEV_YM2610
static DEVDEF_RWFUNC devFunc_MAME_2610[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ym2610_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ym2610_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'A', ym2610_write_pcmroma},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'A', ym2610_alloc_pcmroma},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'B', ym2610_write_pcmromb},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'B', ym2610_alloc_pcmromb},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ym2610_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef_MAME_2610 =
{
	"YM2610", "MAME", FCC_MAME,
	
	device_start_ym2610,
	device_stop_ym2610,
	ym2610_reset_chip,
	ym2610_update_one,
	
	NULL,	// SetOptionBits
	ym2610_set_mute_mask,	// SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback (not required, the YM2610 lacks the "prescaler" register)
	ym2610_set_log_cb,	// SetLoggingCallback
	device_ym2610_link_ssg,	// LinkDevice
	
	devFunc_MAME_2610,	// rwFuncs
};
static DEV_DEF devDef_MAME_2610B =
{
	"YM2610B", "MAME", FCC_MAME,
	
	device_start_ym2610,
	device_stop_ym2610,
	ym2610_reset_chip,
	ym2610b_update_one,
	
	NULL,	// SetOptionBits
	ym2610_set_mute_mask,	 // SetMuteMask
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback (not required, the YM2610 lacks the "prescaler" register)
	ym2610_set_log_cb,	// SetLoggingCallback
	device_ym2610_link_ssg,	// LinkDevice
	
	devFunc_MAME_2610,	// rwFuncs
};

const DEV_DEF* devDefList_YM2610[] =
{
	&devDef_MAME_2610,
	NULL
};
#endif	// SNDDEV_YM2610


static UINT8 get_ssg_funcs(const DEV_DEF* devDef, ssg_callbacks* retFuncs)
{
	UINT8 retVal;
	
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&retFuncs->write);
	if (retVal)
		return retVal;
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, (void**)&retFuncs->read);
	if (retVal)
		return retVal;
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_CLOCK | RWF_WRITE, DEVRW_VALUE, 0, (void**)&retFuncs->set_clock);
	if (retVal)
		return retVal;
	
	if (devDef->Reset == NULL)
		return 0xFF;
	retFuncs->reset = devDef->Reset;
	return 0x00;
}

static AY8910_CFG* get_ssg_config(const DEV_GEN_CFG* cfg, UINT32 clockDiv, UINT8 ssgType)
{
	AY8910_CFG* ssgCfg;
	
	ssgCfg = (AY8910_CFG*)calloc(1, sizeof(AY8910_CFG));
	ssgCfg->_genCfg = *cfg;
	ssgCfg->_genCfg.clock = cfg->clock / clockDiv / 2;
	ssgCfg->_genCfg.flags = 0x00;
	ssgCfg->_genCfg.emuCore = 0;
	ssgCfg->chipType = ssgType;
	ssgCfg->chipFlags = YM2149_PIN26_HIGH;
	
	return ssgCfg;
}

static void init_ssg_devinfo(DEV_INFO* devInf, const DEV_GEN_CFG* baseCfg, UINT8 clockDiv, UINT8 ssgType)
{
	DEVLINK_INFO* devLink;
	
	devInf->linkDevCount = 1;
	devInf->linkDevs = (DEVLINK_INFO*)calloc(devInf->linkDevCount, sizeof(DEVLINK_INFO));
	
	devLink = &devInf->linkDevs[0];
	devLink->devID = DEVID_AY8910;
	devLink->linkID = LINKDEV_SSG;
	devLink->cfg = (DEV_GEN_CFG*)get_ssg_config(baseCfg, clockDiv, ssgType);
	return;
}

#ifdef SNDDEV_YM2203
static UINT8 device_start_ym2203(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	OPN_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	info = (OPN_INF*)malloc(sizeof(OPN_INF));
	info->ssg = NULL;
	info->opn = ym2203_init(info, cfg->clock, rate, NULL, NULL);
	
	devData = (DEV_DATA*)info->opn;
	devData->chipInf = info;	// store pointer to OPN_INF into sound chip structure
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME_2203);
	init_ssg_devinfo(retDevInf, cfg, 1, AYTYPE_YM2203);
	return 0x00;
}

static void device_stop_ym2203(void* param)
{
	DEV_DATA* devData = (DEV_DATA*)param;
	OPN_INF* info = (OPN_INF*)devData->chipInf;
	
	ym2203_shutdown(info->opn);
	free(info);
	
	return;
}

static UINT8 device_ym2203_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG)
{
	ssg_callbacks ssgfunc;
	UINT8 retVal;
	
	if (devID != LINKDEV_SSG)
		return EERR_UNK_DEVICE;
	
	if (defInfSSG == NULL)
	{
		ym2203_link_ssg(param, NULL, NULL);
		retVal = 0x00;
	}
	else
	{
		retVal = get_ssg_funcs(defInfSSG->devDef, &ssgfunc);
		if (! retVal)
			ym2203_link_ssg(param, &ssgfunc, defInfSSG->dataPtr);
	}
	return retVal;
}
#endif	// SNDDEV_YM2203

#ifdef SNDDEV_YM2608
static UINT8 device_start_ym2608(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	OPN_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 2 / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	info = (OPN_INF*)malloc(sizeof(OPN_INF));
	info->ssg = NULL;
	info->opn = ym2608_init(info, cfg->clock, rate, NULL, NULL);
	
	devData = (DEV_DATA*)info->opn;
	devData->chipInf = info;	// store pointer to OPN_INF into sound chip structure
	INIT_DEVINF(retDevInf, devData, rate, &devDef_MAME_2608);
	init_ssg_devinfo(retDevInf, cfg, 2, AYTYPE_YM2608);
	return 0x00;
}

static void device_stop_ym2608(void* param)
{
	DEV_DATA* devData = (DEV_DATA*)param;
	OPN_INF* info = (OPN_INF*)devData->chipInf;
	
	ym2608_shutdown(info->opn);
	free(info);
	
	return;
}

static UINT8 device_ym2608_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG)
{
	ssg_callbacks ssgfunc;
	UINT8 retVal;
	
	if (devID != LINKDEV_SSG)
		return EERR_UNK_DEVICE;
	
	if (defInfSSG == NULL)
	{
		ym2608_link_ssg(param, NULL, NULL);
		retVal = 0x00;
	}
	else
	{
		retVal = get_ssg_funcs(defInfSSG->devDef, &ssgfunc);
		if (! retVal)
			ym2608_link_ssg(param, &ssgfunc, defInfSSG->dataPtr);
	}
	return retVal;
}
#endif	// SNDDEV_YM2608

#ifdef SNDDEV_YM2610
static UINT8 device_start_ym2610(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	OPN_INF* info;
	DEV_DATA* devData;
	UINT32 rate;
	const DEV_DEF* devDefPtr;
	
	rate = cfg->clock / 2 / 72;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	info = (OPN_INF*)malloc(sizeof(OPN_INF));
	info->ssg = NULL;
	info->opn = ym2610_init(info, cfg->clock, rate, NULL, NULL);
	devDefPtr = cfg->flags ? &devDef_MAME_2610B : &devDef_MAME_2610;
	
	devData = (DEV_DATA*)info->opn;
	devData->chipInf = info;	// store pointer to OPN_INF into sound chip structure
	INIT_DEVINF(retDevInf, devData, rate, devDefPtr);
	init_ssg_devinfo(retDevInf, cfg, 2, AYTYPE_YM2610);
	return 0x00;
}

static void device_stop_ym2610(void* param)
{
	DEV_DATA* devData = (DEV_DATA*)param;
	OPN_INF* info = (OPN_INF*)devData->chipInf;
	
	ym2610_shutdown(info->opn);
	free(info);
	
	return;
}

static UINT8 device_ym2610_link_ssg(void* param, UINT8 devID, const DEV_INFO* defInfSSG)
{
	ssg_callbacks ssgfunc;
	UINT8 retVal;
	
	if (devID != LINKDEV_SSG)
		return EERR_UNK_DEVICE;
	
	if (defInfSSG == NULL)
	{
		ym2610_link_ssg(param, NULL, NULL);
		retVal = 0x00;
	}
	else
	{
		retVal = get_ssg_funcs(defInfSSG->devDef, &ssgfunc);
		if (! retVal)
			ym2610_link_ssg(param, &ssgfunc, defInfSSG->dataPtr);
	}
	return retVal;
}
#endif	// SNDDEV_YM2610
