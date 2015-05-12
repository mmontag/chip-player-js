#ifdef WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
int __cdecl _getch(void);	// from conio.h
#else
#define _getch	getchar
#endif

#include "common.h"
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"


int main(int argc, char* argv[]);
static void SetupDirectSound(void* audDrv);
static UINT32 FillBuffer(void* Params, UINT32 bufSize, void* Data);


static UINT32 smplSize;
static void* audDrv;
static void* audDrvLog;

int main(int argc, char* argv[])
{
	UINT8 retVal;
	UINT32 drvCount;
	UINT32 curDrv;
	UINT32 idWavOut;
	UINT32 idWavOutDev;
	UINT32 idWavWrt;
	AUDDRV_INFO* drvInfo;
	AUDIO_OPTS* opts;
	AUDIO_OPTS* optsLog;
	const AUDIO_DEV_LIST* devList;
	
	Audio_Init();
	drvCount = Audio_GetDriverCount();
	if (! drvCount)
		goto Exit_Deinit;
	
#if 0
	idWavOut = (UINT32)-1;
	idWavOutDev = 0;
	idWavWrt = (UINT32)-1;
	for (curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		Audio_GetDriverInfo(curDrv, &drvInfo);
		if (drvInfo->drvType == ADRVTYPE_OUT /*&& idWavOut == (UINT32)-1*/)
			idWavOut = curDrv;
		else if (drvInfo->drvType == ADRVTYPE_DISK && idWavWrt == (UINT32)-1)
			idWavWrt = curDrv;
	}
	if (idWavOut == (UINT32)-1)
	{
		printf("Unable to find output driver\n");
		goto Exit_Deinit;
	}
#endif
	
	printf("Available Drivers:\n");
	for (curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		Audio_GetDriverInfo(curDrv, &drvInfo);
		printf("    Driver %u: Type: %02X, Sig: %02X, Name: %s\n",
				curDrv, drvInfo->drvType, drvInfo->drvSig, drvInfo->drvName);
	}
	if (argc <= 1)
	{
		Audio_Deinit();
		printf("Usage:\n");
		printf("audiotest DriverID [DeviceID] [WaveLogDriverID]\n");
		return 0;
	}
	
	idWavOut = (UINT32)-1;
	idWavOutDev = 0;
	idWavWrt = (UINT32)-1;
	if (argc >= 2)
		idWavOut = strtoul(argv[1], NULL, 0);
	if (argc >= 3)
		idWavOutDev = strtoul(argv[2], NULL, 0);
	if (argc >= 4)
		idWavWrt = strtoul(argv[3], NULL, 0);
	if (idWavOut >= drvCount)
		idWavOut = drvCount - 1;
	
	Audio_GetDriverInfo(idWavOut, &drvInfo);
	printf("Using driver %s.\n", drvInfo->drvName);
	retVal = AudioDrv_Init(idWavOut, &audDrv);
	if (retVal)
	{
		printf("WaveOut: Drv Init Error: %02X\n", retVal);
		goto Exit_Deinit;
	}
	if (drvInfo->drvSig == ADRVSIG_DSOUND)
		SetupDirectSound(audDrv);
	
	if (idWavWrt == (UINT32)-1)
	{
		audDrvLog = NULL;
	}
	else
	{
		void* aDrv;
		
		Audio_GetDriverInfo(idWavWrt, &drvInfo);
		retVal = AudioDrv_Init(idWavWrt, &audDrvLog);
		if (retVal)
		{
			printf("DiskWrt: Drv Init Error: %02X\n", retVal);
			audDrvLog = NULL;
		}
		if (audDrvLog != NULL)
		{
			if (drvInfo->drvSig == ADRVSIG_WAVEWRT)
			{
				aDrv = AudioDrv_GetDrvData(audDrvLog);
				WavWrt_SetFileName(aDrv, "waveOut.wav");
			}
			else if (drvInfo->drvSig == ADRVSIG_DSOUND)
			{
				SetupDirectSound(audDrvLog);
			}
		}
	}
	
	opts = AudioDrv_GetOptions(audDrv);
	opts->numChannels = 1;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	if (audDrvLog != NULL)
	{
		optsLog = AudioDrv_GetOptions(audDrvLog);
		*optsLog = *opts;
	}
	
	devList = AudioDrv_GetDeviceList(audDrv);
	printf("%u devices found.\n", devList->devCount);
	for (curDrv = 0; curDrv < devList->devCount; curDrv ++)
		printf("    Device %u: %s\n", curDrv, devList->devNames[curDrv]);
	
	AudioDrv_SetCallback(audDrv, FillBuffer);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_DrvDeinit;
	}
	
	if (audDrvLog != NULL)
	{
		retVal = AudioDrv_Start(audDrvLog, 0);
		if (retVal)
		{
			printf("DiskWrt: Dev Init Error: %02X\n", retVal);
			AudioDrv_Deinit(&audDrvLog);
		}
		if (audDrvLog != NULL)
			AudioDrv_DataForward_Add(audDrv, audDrvLog);
	}
	
	getchar();
	retVal = AudioDrv_Stop(audDrv);
	if (audDrvLog != NULL)
		retVal = AudioDrv_Stop(audDrvLog);
	
Exit_DrvDeinit:
	AudioDrv_Deinit(&audDrv);
	if (audDrvLog != NULL)
		AudioDrv_Deinit(&audDrvLog);
Exit_Deinit:
	Audio_Deinit();
	printf("Done.\n");
	//_getch();
	return 0;
}

static UINT32 FillBuffer(void* Params, UINT32 bufSize, void* data)
{
	UINT32 smplCount;
	INT16* SmplPtr;
	UINT32 curSmpl;
	
	smplCount = bufSize / smplSize;
	SmplPtr = (INT16*)data;
	for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
	{
		if ((curSmpl / (smplCount / 16)) < 15)
			SmplPtr[curSmpl] = +0x1000;
		else
			SmplPtr[curSmpl] = -0x1000;
	}
	
	return smplCount * smplSize;
}

static void SetupDirectSound(void* audDrv)
{
#ifdef WIN32
	void* aDrv;
	HWND hWndConsole;
	
	aDrv = AudioDrv_GetDrvData(audDrv);
#if _WIN32_WINNT >= 0x500
	hWndConsole = GetConsoleWindow();
#else
	hWndConsole = GetDesktopWindow();	// not as nice, but works
#endif
	DSound_SetHWnd(aDrv, hWndConsole);
#endif	// WIN32
	
	return;
}
