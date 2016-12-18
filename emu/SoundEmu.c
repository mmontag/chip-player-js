#include <stddef.h>
#include <stdlib.h>	// for malloc/free

#include <stdtype.h>
#include "EmuStructs.h"
#include "SoundEmu.h"
#include "SoundDevs.h"

#include "cores/sn764intf.h"
#include "cores/2413intf.h"
#include "cores/2612intf.h"
#include "cores/okim6295.h"

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
	case DEVID_OKIM6295:
		return devDefList_OKIM6295;
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
			return (*curDIL)->Start(cfg, retDevInf);
	}
	return EERR_UNK_DEVICE;
}

UINT8 SndEmu_Stop(DEV_INFO* devInf)
{
	devInf->devDef->Stop(devInf->dataPtr);
	devInf->dataPtr = NULL;
	
	return 0x00;
}

UINT8 SndEmu_GetDeviceFunc(const DEV_DEF* devDef, UINT8 funcType, UINT8 rwType, UINT16 reserved, void** retFuncPtr)
{
	UINT32 curFunc;
	const DEVDEF_RWFUNC* tempFnc;
	UINT32 firstFunc;
	UINT32 foundFunc;
	
	foundFunc = 0;
	firstFunc = 0;
	for (curFunc = 0; curFunc < devDef->rwFuncCount; curFunc ++)
	{
		tempFnc = &devDef->rwFuncs[curFunc];
		if (tempFnc->funcType == funcType && tempFnc->rwType == rwType)
		{
			if (foundFunc == 0)
				firstFunc = curFunc;
			foundFunc ++;
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
