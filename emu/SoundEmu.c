#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include <stdtype.h>
#include "EmuStructs.h"
#include "SoundEmu.h"
#include "SoundDevs.h"

#include "cores/sn764intf.h"
#include "cores/2413intf.h"
#include "cores/2612intf.h"
#include "cores/ym2151.h"
#include "cores/segapcm.h"
#include "cores/rf5cintf.h"
#include "cores/opnintf.h"
#include "cores/oplintf.h"
#include "cores/262intf.h"
#include "cores/ymf278b.h"
#include "cores/ymf271.h"
#include "cores/ymz280b.h"
#include "cores/pwm.h"
#include "cores/ayintf.h"
#include "cores/gb.h"
#include "cores/nesintf.h"
#include "cores/multipcm.h"
#include "cores/upd7759.h"
#include "cores/okim6258.h"
#include "cores/okim6295.h"
#include "cores/k051649.h"
#include "cores/k054539.h"
#include "cores/k053260.h"
#include "cores/c140.h"

const DEV_DEF** SndEmu_GetDevDefList(UINT8 deviceID)
{
	switch(deviceID)
	{
	case DEVID_SN76496:
		return devDefList_SN76496;
	case DEVID_YM2413:
		return devDefList_YM2413;
	case DEVID_YM2612:
		return devDefList_YM2612;
	case DEVID_YM2151:
		return devDefList_YM2151;
	case DEVID_SEGAPCM:
		return devDefList_SegaPCM;
	case DEVID_RF5C68:
		return devDefList_RF5C68;
	case DEVID_YM2203:
		return devDefList_YM2203;
	case DEVID_YM2608:
		return devDefList_YM2608;
	case DEVID_YM2610:
		return devDefList_YM2610;
	case DEVID_YM3812:
		return devDefList_YM3812;
	case DEVID_YM3526:
		return devDefList_YM3526;
	case DEVID_Y8950:
		return devDefList_Y8950;
	case DEVID_YMF262:
		return devDefList_YMF262;
	case DEVID_YMF278B:
		return devDefList_YMF278B;
	case DEVID_YMF271:
		return devDefList_YMF271;
	case DEVID_YMZ280B:
		return devDefList_YMZ280B;
	case DEVID_32X_PWM:
		return devDefList_32X_PWM;
	case DEVID_AY8910:
		return devDefList_AY8910;
	case DEVID_GB_DMG:
		return devDefList_GB_DMG;
	case DEVID_NES_APU:
		return devDefList_NES_APU;
	case DEVID_MULTIPCM:
		return devDefList_MultiPCM;
	case DEVID_uPD7759:
		return devDefList_uPD7759;
	case DEVID_OKIM6258:
		return devDefList_OKIM6258;
	case DEVID_OKIM6295:
		return devDefList_OKIM6295;
	case DEVID_K051649:
		return devDefList_K051649;
	case DEVID_K054539:
		return devDefList_K054539;
	case DEVID_K053260:
		return devDefList_K053260;
	case DEVID_C140:
		return devDefList_C140;
	}
	return NULL;
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
	return EERR_UNK_DEVICE;
}

UINT8 SndEmu_Stop(DEV_INFO* devInf)
{
	devInf->devDef->Stop(devInf->dataPtr);
	devInf->dataPtr = NULL;
	
	return 0x00;
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

UINT8 SndEmu_GetDeviceFunc(const DEV_DEF* devDef, UINT8 funcType, UINT8 rwType, UINT16 reserved, void** retFuncPtr)
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
			if (! reserved || reserved == tempFnc->user)
			{
				if (foundFunc == 0)
					firstFunc = curFunc;
				foundFunc ++;
			}
		}
	}
	if (foundFunc == 0)
		return 0xFF;	// not found
	*retFuncPtr = devDef->rwFuncs[firstFunc].funcPtr;
	if (foundFunc == 1)
		return 0x00;
	else
		return 0x01;	// found multiple matching functions
}
