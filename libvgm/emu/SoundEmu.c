#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include "../stdtype.h"
#include "EmuStructs.h"
#include "SoundEmu.h"
#include "SoundDevs.h"

#ifndef SNDDEV_SELECT
// if not asked to select certain sound devices, just include everything (comfort option)
#define SNDDEV_SN76496
#define SNDDEV_YM2413
#define SNDDEV_YM2612
#define SNDDEV_YM2151
#define SNDDEV_SEGAPCM
#define SNDDEV_RF5C68
#define SNDDEV_YM2203
#define SNDDEV_YM2608
#define SNDDEV_YM2610
#define SNDDEV_YM3812
#define SNDDEV_YM3526
#define SNDDEV_Y8950
#define SNDDEV_YMF262
#define SNDDEV_YMF278B
#define SNDDEV_YMZ280B
#define SNDDEV_YMF271
#define SNDDEV_AY8910
#define SNDDEV_32X_PWM
#define SNDDEV_GAMEBOY
#define SNDDEV_NES_APU
#define SNDDEV_YMW258
#define SNDDEV_UPD7759
#define SNDDEV_OKIM6258
#define SNDDEV_OKIM6295
#define SNDDEV_K051649
#define SNDDEV_K054539
#define SNDDEV_C6280
#define SNDDEV_C140
#define SNDDEV_C219
#define SNDDEV_K053260
#define SNDDEV_POKEY
#define SNDDEV_QSOUND
#define SNDDEV_SCSP
#define SNDDEV_WSWAN
#define SNDDEV_VBOY_VSU
#define SNDDEV_SAA1099
#define SNDDEV_ES5503
#define SNDDEV_ES5506
#define SNDDEV_X1_010
#define SNDDEV_C352
#define SNDDEV_GA20
#define SNDDEV_MIKEY
#endif

#ifdef SNDDEV_SN76496
#include "cores/sn764intf.h"
#endif
#ifdef SNDDEV_YM2413
#include "cores/2413intf.h"
#endif
#ifdef SNDDEV_YM2612
#include "cores/2612intf.h"
#endif
#ifdef SNDDEV_YM2151
#include "cores/2151intf.h"
#endif
#ifdef SNDDEV_SEGAPCM
#include "cores/segapcm.h"
#endif
#ifdef SNDDEV_RF5C68
#include "cores/rf5cintf.h"
#endif
#if defined(SNDDEV_YM2203) || defined(SNDDEV_YM2608) || defined(SNDDEV_YM2610)
#include "cores/opnintf.h"
#endif
#if defined(SNDDEV_YM3812) || defined(SNDDEV_YM3526) || defined(SNDDEV_Y8950)
#include "cores/oplintf.h"
#endif
#ifdef SNDDEV_YMF262
#include "cores/262intf.h"
#endif
#ifdef SNDDEV_YMF278B
#include "cores/ymf278b.h"
#endif
#ifdef SNDDEV_YMF271
#include "cores/ymf271.h"
#endif
#ifdef SNDDEV_YMZ280B
#include "cores/ymz280b.h"
#endif
#ifdef SNDDEV_32X_PWM
#include "cores/pwm.h"
#endif
#ifdef SNDDEV_AY8910
#include "cores/ayintf.h"
#endif
#ifdef SNDDEV_GAMEBOY
#include "cores/gb.h"
#endif
#ifdef SNDDEV_NES_APU
#include "cores/nesintf.h"
#endif
#ifdef SNDDEV_YMW258
#include "cores/multipcm.h"
#endif
#ifdef SNDDEV_UPD7759
#include "cores/upd7759.h"
#endif
#ifdef SNDDEV_OKIM6258
#include "cores/okim6258.h"
#endif
#ifdef SNDDEV_OKIM6295
#include "cores/okim6295.h"
#endif
#ifdef SNDDEV_K051649
#include "cores/k051649.h"
#endif
#ifdef SNDDEV_K054539
#include "cores/k054539.h"
#endif
#ifdef SNDDEV_C6280
#include "cores/c6280intf.h"
#endif
#ifdef SNDDEV_C140
#include "cores/c140.h"
#endif
#ifdef SNDDEV_C219
#include "cores/c219.h"
#endif
#ifdef SNDDEV_K053260
#include "cores/k053260.h"
#endif
#ifdef SNDDEV_POKEY
#include "cores/pokey.h"
#endif
#ifdef SNDDEV_QSOUND
#include "cores/qsoundintf.h"
#endif
#ifdef SNDDEV_SCSP
#include "cores/scsp.h"
#endif
#ifdef SNDDEV_WSWAN
#include "cores/ws_audio.h"
#endif
#ifdef SNDDEV_VBOY_VSU
#include "cores/vsu.h"
#endif
#ifdef SNDDEV_SAA1099
#include "cores/saaintf.h"
#endif
#ifdef SNDDEV_ES5503
#include "cores/es5503.h"
#endif
#ifdef SNDDEV_ES5506
#include "cores/es5506.h"
#endif
#ifdef SNDDEV_X1_010
#include "cores/x1_010.h"
#endif
#ifdef SNDDEV_C352
#include "cores/c352.h"
#endif
#ifdef SNDDEV_GA20
#include "cores/iremga20.h"
#endif
#ifdef SNDDEV_MIKEY
#include "cores/mikey.h"
#endif

const DEV_DEF** SndEmu_GetDevDefList(UINT8 deviceID)
{
	switch(deviceID)
	{
#ifdef SNDDEV_SN76496
	case DEVID_SN76496:
		return devDefList_SN76496;
#endif
#ifdef SNDDEV_YM2413
	case DEVID_YM2413:
		return devDefList_YM2413;
#endif
#ifdef SNDDEV_YM2612
	case DEVID_YM2612:
		return devDefList_YM2612;
#endif
#ifdef SNDDEV_YM2151
	case DEVID_YM2151:
		return devDefList_YM2151;
#endif
#ifdef SNDDEV_SEGAPCM
	case DEVID_SEGAPCM:
		return devDefList_SegaPCM;
#endif
#ifdef SNDDEV_RF5C68
	case DEVID_RF5C68:
		return devDefList_RF5C68;
#endif
#ifdef SNDDEV_YM2203
	case DEVID_YM2203:
		return devDefList_YM2203;
#endif
#ifdef SNDDEV_YM2608
	case DEVID_YM2608:
		return devDefList_YM2608;
#endif
#ifdef SNDDEV_YM2610
	case DEVID_YM2610:
		return devDefList_YM2610;
#endif
#ifdef SNDDEV_YM3812
	case DEVID_YM3812:
		return devDefList_YM3812;
#endif
#ifdef SNDDEV_YM3526
	case DEVID_YM3526:
		return devDefList_YM3526;
#endif
#ifdef SNDDEV_Y8950
	case DEVID_Y8950:
		return devDefList_Y8950;
#endif
#ifdef SNDDEV_YMF262
	case DEVID_YMF262:
		return devDefList_YMF262;
#endif
#ifdef SNDDEV_YMF278B
	case DEVID_YMF278B:
		return devDefList_YMF278B;
#endif
#ifdef SNDDEV_YMF271
	case DEVID_YMF271:
		return devDefList_YMF271;
#endif
#ifdef SNDDEV_YMZ280B
	case DEVID_YMZ280B:
		return devDefList_YMZ280B;
#endif
#ifdef SNDDEV_32X_PWM
	case DEVID_32X_PWM:
		return devDefList_32X_PWM;
#endif
#ifdef SNDDEV_AY8910
	case DEVID_AY8910:
		return devDefList_AY8910;
#endif
#ifdef SNDDEV_GAMEBOY
	case DEVID_GB_DMG:
		return devDefList_GB_DMG;
#endif
#ifdef SNDDEV_NES_APU
	case DEVID_NES_APU:
		return devDefList_NES_APU;
#endif
#ifdef SNDDEV_YMW258
	case DEVID_YMW258:
		return devDefList_YMW258;
#endif
#ifdef SNDDEV_UPD7759
	case DEVID_uPD7759:
		return devDefList_uPD7759;
#endif
#ifdef SNDDEV_OKIM6258
	case DEVID_OKIM6258:
		return devDefList_OKIM6258;
#endif
#ifdef SNDDEV_OKIM6295
	case DEVID_OKIM6295:
		return devDefList_OKIM6295;
#endif
#ifdef SNDDEV_K051649
	case DEVID_K051649:
		return devDefList_K051649;
#endif
#ifdef SNDDEV_K054539
	case DEVID_K054539:
		return devDefList_K054539;
#endif
#ifdef SNDDEV_C6280
	case DEVID_C6280:
		return devDefList_C6280;
#endif
#ifdef SNDDEV_C140
	case DEVID_C140:
		return devDefList_C140;
#endif
#ifdef SNDDEV_C219
	case DEVID_C219:
		return devDefList_C219;
#endif
#ifdef SNDDEV_K053260
	case DEVID_K053260:
		return devDefList_K053260;
#endif
#ifdef SNDDEV_POKEY
	case DEVID_POKEY:
		return devDefList_Pokey;
#endif
#ifdef SNDDEV_QSOUND
	case DEVID_QSOUND:
		return devDefList_QSound;
#endif
#ifdef SNDDEV_SCSP
	case DEVID_SCSP:
		return devDefList_SCSP;
#endif
#ifdef SNDDEV_WSWAN
	case DEVID_WSWAN:
		return devDefList_WSwan;
#endif
#ifdef SNDDEV_VBOY_VSU
	case DEVID_VBOY_VSU:
		return devDefList_VBoyVSU;
#endif
#ifdef SNDDEV_SAA1099
	case DEVID_SAA1099:
		return devDefList_SAA1099;
#endif
#ifdef SNDDEV_ES5503
	case DEVID_ES5503:
		return devDefList_ES5503;
#endif
#ifdef SNDDEV_ES5506
	case DEVID_ES5506:
		return devDefList_ES5506;
#endif
#ifdef SNDDEV_X1_010
	case DEVID_X1_010:
		return devDefList_X1_010;
#endif
#ifdef SNDDEV_C352
	case DEVID_C352:
		return devDefList_C352;
#endif
#ifdef SNDDEV_GA20
	case DEVID_GA20:
		return devDefList_GA20;
#endif
#ifdef SNDDEV_MIKEY
	case DEVID_MIKEY:
		return devDefList_Mikey;
#endif
	default:
		return NULL;
	}
}

UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	const DEV_DEF** diList;
	const DEV_DEF** curDIL;
	
	diList = SndEmu_GetDevDefList(deviceID);
	if (diList == NULL)
		return EERR_UNK_DEVICE;
	
	for (curDIL = diList; *curDIL != NULL; curDIL ++)
	{
		// emuCore == 0 -> use default
		if (! cfg->emuCore || (*curDIL)->coreID == cfg->emuCore)
		{
			UINT8 retVal;
			
			retVal = (*curDIL)->Start(cfg, retDevInf);
			if (! retVal)	// if initialization is successful, reset the chip to ensure a clean state
				(*curDIL)->Reset(retDevInf->dataPtr);
			return retVal;
		}
	}
	return EERR_NOT_FOUND;
}

UINT8 SndEmu_Stop(DEV_INFO* devInf)
{
	devInf->devDef->Stop(devInf->dataPtr);
	devInf->dataPtr = NULL;
	
	return EERR_OK;
}

void SndEmu_FreeDevLinkData(DEV_INFO* devInf)
{
	UINT32 curLDev;
	
	if (! devInf->linkDevCount)
		return;
	
	for (curLDev = 0; curLDev < devInf->linkDevCount; curLDev ++)
		free(devInf->linkDevs[curLDev].cfg);
	free(devInf->linkDevs);	devInf->linkDevs = NULL;
	devInf->linkDevCount = 0;
	
	return;
}

UINT8 SndEmu_GetDeviceFunc(const DEV_DEF* devDef, UINT8 funcType, UINT8 rwType, UINT16 user, void** retFuncPtr)
{
	UINT32 curFunc;
	const DEVDEF_RWFUNC* tempFnc;
	UINT32 firstFunc;
	UINT32 foundFunc;
	
	foundFunc = 0;
	firstFunc = 0;
	for (curFunc = 0; devDef->rwFuncs[curFunc].funcPtr != NULL; curFunc ++)
	{
		tempFnc = &devDef->rwFuncs[curFunc];
		if (tempFnc->funcType == funcType && tempFnc->rwType == rwType)
		{
			if (! user || user == tempFnc->user)
			{
				if (foundFunc == 0)
					firstFunc = curFunc;
				foundFunc ++;
			}
		}
	}
	if (foundFunc == 0)
		return EERR_NOT_FOUND;
	*retFuncPtr = devDef->rwFuncs[firstFunc].funcPtr;
	if (foundFunc == 1)
		return EERR_OK;
	else
		return EERR_MORE_FOUND;	// found multiple matching functions
}

// opts:
//	0x01: long names (1) / short names (0)
const char* SndEmu_GetDevName(UINT8 deviceID, UINT8 opts, const DEV_GEN_CFG* devCfg)
{
	if (! (opts & 0x01))
		devCfg = NULL;	// devCfg only has an effect when "long names" are enabled
	switch(deviceID)
	{
#ifdef SNDDEV_SN76496
	case DEVID_SN76496:
		if (devCfg != NULL)
		{
			const SN76496_CFG* snCfg = (const SN76496_CFG*)devCfg;
			if (snCfg->_genCfg.flags)
				return "T6W28";
			switch(snCfg->shiftRegWidth)
			{
			case 0x0F:
				return (snCfg->clkDiv == 1) ? "SN94624" : "SN76489";
			case 0x10:
				if (snCfg->noiseTaps == 0x0009)
					return "SEGA PSG";
				else if (snCfg->noiseTaps == 0x0022)
				{
					if (snCfg->ncrPSG)	// Tandy noise mode
						return snCfg->negate ? "NCR8496" : "PSSJ-3";
					else
						return "NCR8496";
				}
				break;
			case 0x11:
				return (snCfg->clkDiv == 1) ? "SN76494" : "SN76489A";
			default:
				return "SN764xx";
			}
		}
		return "SN76496";
#endif
#ifdef SNDDEV_YM2413
	case DEVID_YM2413:
		if (devCfg != NULL && devCfg->flags)
			return "VRC7";
		return "YM2413";
#endif
#ifdef SNDDEV_YM2612
	case DEVID_YM2612:
		if (devCfg != NULL && devCfg->flags)
			return "YM3438";
		return "YM2612";
#endif
#ifdef SNDDEV_YM2151
	case DEVID_YM2151:
		return "YM2151";
#endif
#ifdef SNDDEV_SEGAPCM
	case DEVID_SEGAPCM:
		if (opts & 0x01)
			return "Sega PCM";
		return "SegaPCM";
#endif
#ifdef SNDDEV_RF5C68
	case DEVID_RF5C68:
		if (devCfg != NULL)
		{
			if (devCfg->flags == 1)
				return "RF5C164";
			else if (devCfg->flags == 2)
				return "RF5C105";
		}
		return "RF5C68";
#endif
#ifdef SNDDEV_YM2203
	case DEVID_YM2203:
		return "YM2203";
#endif
#ifdef SNDDEV_YM2608
	case DEVID_YM2608:
		return "YM2608";
#endif
#ifdef SNDDEV_YM2610
	case DEVID_YM2610:
		if (devCfg != NULL && devCfg->flags)
			return "YM2610B";
		return "YM2610";
#endif
#ifdef SNDDEV_YM3812
	case DEVID_YM3812:
		return "YM3812";
#endif
#ifdef SNDDEV_YM3526
	case DEVID_YM3526:
		return "YM3526";
#endif
#ifdef SNDDEV_Y8950
	case DEVID_Y8950:
		return "Y8950";
#endif
#ifdef SNDDEV_YMF262
	case DEVID_YMF262:
		return "YMF262";
#endif
#ifdef SNDDEV_YMF278B
	case DEVID_YMF278B:
		return "YMF278B";
#endif
#ifdef SNDDEV_YMF271
	case DEVID_YMF271:
		return "YMF271";
#endif
#ifdef SNDDEV_YMZ280B
	case DEVID_YMZ280B:
		return "YMZ280B";
#endif
#ifdef SNDDEV_32X_PWM
	case DEVID_32X_PWM:
		return "32X PWM";
#endif
#ifdef SNDDEV_AY8910
	case DEVID_AY8910:
		if (devCfg != NULL)
		{
			const AY8910_CFG* ayCfg = (const AY8910_CFG*)devCfg;
			switch(ayCfg->chipType)
			{
			case 0x00:
				return "AY-3-8910";
			case 0x01:
				return "AY-3-8912";
			case 0x02:
				return "AY-3-8913";
			case 0x03:
				return "AY8930";
			case 0x04:
				return "AY-3-8914";
			case 0x10:
				return "YM2149";
			case 0x11:
				return "YM3439";
			case 0x12:
				return "YMZ284";
			case 0x13:
				return "YMZ294";
			}
		}
		return "AY8910";
#endif
#ifdef SNDDEV_GAMEBOY
	case DEVID_GB_DMG:
		if (opts & 0x01)
			return "GameBoy DMG";
		return "GB DMG";
#endif
#ifdef SNDDEV_NES_APU
	case DEVID_NES_APU:
		if (devCfg != NULL && devCfg->flags)
			return "NES APU + FDS";
		return "NES APU";
#endif
#ifdef SNDDEV_YMW258
	case DEVID_YMW258:
		return "YMW258";
#endif
#ifdef SNDDEV_UPD7759
	case DEVID_uPD7759:
		return "uPD7759";
#endif
#ifdef SNDDEV_OKIM6258
	case DEVID_OKIM6258:
		return "OKIM6258";
#endif
#ifdef SNDDEV_OKIM6295
	case DEVID_OKIM6295:
		return "OKIM6295";
#endif
#ifdef SNDDEV_K051649
	case DEVID_K051649:
		if (devCfg != NULL && devCfg->flags)
			return "K052539";
		return "K051649";
#endif
#ifdef SNDDEV_K054539
	case DEVID_K054539:
		return "K054539";
#endif
#ifdef SNDDEV_C6280
	case DEVID_C6280:
		return "C6280";
#endif
#ifdef SNDDEV_C140
	case DEVID_C140:
		return "C140";
#endif
#ifdef SNDDEV_C219
	case DEVID_C219:
		return "C219";
#endif
#ifdef SNDDEV_K053260
	case DEVID_K053260:
		return "K053260";
#endif
#ifdef SNDDEV_POKEY
	case DEVID_POKEY:
		return "Pokey";
#endif
#ifdef SNDDEV_QSOUND
	case DEVID_QSOUND:
		return "QSound";
#endif
#ifdef SNDDEV_SCSP
	case DEVID_SCSP:
		return "SCSP";
#endif
#ifdef SNDDEV_WSWAN
	case DEVID_WSWAN:
		if (opts & 0x01)
			return "WonderSwan";
		return "WSwan";
#endif
#ifdef SNDDEV_VBOY_VSU
	case DEVID_VBOY_VSU:
		return "VBoy VSU";
#endif
#ifdef SNDDEV_SAA1099
	case DEVID_SAA1099:
		return "SAA1099";
#endif
#ifdef SNDDEV_ES5503
	case DEVID_ES5503:
		return "ES5503";
#endif
#ifdef SNDDEV_ES5506
	case DEVID_ES5506:
		if (devCfg != NULL && ! devCfg->flags)
			return "ES5505";
		return "ES5506";
#endif
#ifdef SNDDEV_X1_010
	case DEVID_X1_010:
		return "X1-010";
#endif
#ifdef SNDDEV_C352
	case DEVID_C352:
		return "C352";
#endif
#ifdef SNDDEV_GA20
	case DEVID_GA20:
		return "GA20";
#endif
#ifdef SNDDEV_MIKEY
	case DEVID_MIKEY:
		return "MIKEY";
#endif
	default:
		return NULL;
	}
}
