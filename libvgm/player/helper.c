#include <stdlib.h>
#include <string.h>

#include "../stdtype.h"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"
#include "../emu/Resampler.h"
#include "helper.h"

void SetupLinkedDevices(VGM_BASEDEV* cBaseDev, SETUPLINKDEV_CB devCfgCB, void* cbUserParam)
{
	UINT32 curLDev;
	UINT8 retVal;
	VGM_BASEDEV* cParent;
	
	if (cBaseDev->defInf.linkDevCount == 0 || cBaseDev->defInf.devDef->LinkDevice == NULL)
		return;
	
	cParent = NULL;
	for (curLDev = 0; curLDev < cBaseDev->defInf.linkDevCount; curLDev ++)
	{
		VGM_BASEDEV* cDevCur;
		DEVLINK_INFO* dLink;
		
		dLink = &cBaseDev->defInf.linkDevs[curLDev];
		cDevCur = (VGM_BASEDEV*)calloc(1, sizeof(VGM_BASEDEV));
		if (cDevCur == NULL)
			break;
		cDevCur->linkDev = NULL;
		if (cParent == NULL)
			cBaseDev->linkDev = cDevCur;
		else
			cParent->linkDev = cDevCur;
		
		// do a callback to allow for additional configuration of child-devices
		if (devCfgCB != NULL)
			devCfgCB(cbUserParam, cDevCur, dLink);
		
		retVal = SndEmu_Start(dLink->devID, dLink->cfg, &cDevCur->defInf);
		if (retVal)
		{
			free(cBaseDev->linkDev);
			cBaseDev->linkDev = NULL;
			break;
		}
		cBaseDev->defInf.devDef->LinkDevice(cBaseDev->defInf.dataPtr, dLink->linkID, &cDevCur->defInf);
		cParent = cDevCur;
	}
	
	return;
}

void FreeDeviceTree(VGM_BASEDEV* cBaseDev, UINT8 freeBase)
{
	VGM_BASEDEV* cDevCur;
	VGM_BASEDEV* cDevOld;
	
	cDevCur = cBaseDev;
	while(cDevCur != NULL)
	{
		if (cDevCur->defInf.dataPtr != NULL)
		{
			Resmpl_Deinit(&cDevCur->resmpl);
			SndEmu_Stop(&cDevCur->defInf);
		}
		SndEmu_FreeDevLinkData(&cDevCur->defInf);
		cDevOld = cDevCur;
		cDevCur = cDevCur->linkDev;
		if (cDevOld == cBaseDev && ! freeBase)
			cDevOld->linkDev = NULL;
		else
			free(cDevOld);
	}
	
	return;
}
