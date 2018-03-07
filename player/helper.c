#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <iconv.h>

#include <stdtype.h>
#include <emu/EmuStructs.h>
#include <emu/SoundEmu.h>
#include <emu/Resampler.h>
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

// parameters:
//	hIConv: iconv_t handle
//	outSize: [input] size of output buffer, [output] size of converted string
//	outStr: [input/output] pointer to output buffer, if data is NULL, it will be allocated automatically
//	inSize: [input] length of input string, may be 0 (in that case, strlen() is used to determine the string's length)
//	inStr: [input] input string
UINT8 StrCharsetConv(iconv_t hIConv, size_t* outSize, char** outStr, size_t inSize, const char* inStr)
{
	size_t outBufSize;
	size_t remBytesIn;
	size_t remBytesOut;
	char* inPtr;
	char* outPtr;
	size_t wrtBytes;
	char resVal;
	
	iconv(hIConv, NULL, NULL, NULL, NULL);	// reset conversion state
	
	if (hIConv == NULL)
		return 0xFF;	// bad input
	
	remBytesIn = inSize ? inSize : strlen(inStr);
	inPtr = (char*)&inStr[0];	// const-cast due to a bug in the API
	if (remBytesIn == 0)
	{
		*outSize = 0;
		return 0x02;	// nothing to convert
	}
	
	if (*outStr == NULL)
	{
		outBufSize = remBytesIn * 3 / 2;
		*outStr = (char*)malloc(outBufSize);
	}
	else
	{
		outBufSize = *outSize;
	}
	remBytesOut = outBufSize;
	outPtr = *outStr;
	
	resVal = 0x00;
	wrtBytes = iconv(hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	while(wrtBytes == (size_t)-1)
	{
		if (errno == EILSEQ || errno == EINVAL)
		{
			// invalid encoding
			resVal = 0x80;
			if (errno == EINVAL && remBytesIn <= 1)
			{
				// assume that the string got truncated
				iconv(hIConv, NULL, NULL, &outPtr, &remBytesOut);
				resVal = 0x01;
			}
			break;
		}
		// errno == E2BIG
		wrtBytes = outPtr - *outStr;
		outBufSize += remBytesIn * 2;
		*outStr = (char*)realloc(*outStr, outBufSize);
		outPtr = *outStr + wrtBytes;
		remBytesOut = outBufSize - wrtBytes;
		
		wrtBytes = iconv(hIConv, &inPtr, &remBytesIn, &outPtr, &remBytesOut);
	}
	
	*outSize = outPtr - *outStr;
	return resVal;
}
