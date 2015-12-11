#ifdef _WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
int __cdecl _getch(void);	// from conio.h
#else
#define _getch	getchar
#endif

#include <common_def.h>
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/EmuStructs.h"
#include "emu/SoundEmu.h"
#include "emu/sn764intf.h"


int main(int argc, char* argv[]);
static void SetupDirectSound(void* audDrv);
static UINT32 FillBuffer(void* Params, UINT32 bufSize, void* Data);


static UINT32 smplSize;
static void* audDrv;
static DEV_INFO snDefInf;
static UINT32 smplAlloc;
static DEV_SMPL* smplData[2];

int main(int argc, char* argv[])
{
	UINT8 retVal;
	UINT32 drvCount;
	UINT32 idWavOut;
	UINT32 idWavOutDev;
	AUDDRV_INFO* drvInfo;
	AUDIO_OPTS* opts;
	
	DEV_GEN_CFG devCfg;
	SN76496_CFG snCfg;
	//DEV_INFO snDefInf;
	DEVFUNC_WRITE_A8D8 snWrite;
	UINT8 curReg;
	
	Audio_Init();
	drvCount = Audio_GetDriverCount();
	if (! drvCount)
		goto Exit_AudDeinit;
	
	if (argc <= 1)
	{
		Audio_Deinit();
		printf("Usage:\n");
		printf("audemutest DriverID [DeviceID]\n");
		return 0;
	}
	
	idWavOut = (UINT32)-1;
	idWavOutDev = 0;
	if (argc >= 2)
		idWavOut = strtoul(argv[1], NULL, 0);
	if (argc >= 3)
		idWavOutDev = strtoul(argv[2], NULL, 0);
	if (idWavOut >= drvCount)
		idWavOut = drvCount - 1;
	
	Audio_GetDriverInfo(idWavOut, &drvInfo);
	printf("Using driver %s.\n", drvInfo->drvName);
	retVal = AudioDrv_Init(idWavOut, &audDrv);
	if (retVal)
	{
		printf("WaveOut: Drv Init Error: %02X\n", retVal);
		goto Exit_AudDeinit;
	}
	if (drvInfo->drvSig == ADRVSIG_DSOUND)
		SetupDirectSound(audDrv);
	
	devCfg.emuCore = 0;
	devCfg.srMode = DEVRI_SRMODE_NATIVE;
	devCfg.clock = 3579545;
	devCfg.smplRate = 48000;
	snCfg._genCfg = devCfg;
	snCfg.shiftRegWidth = 0x10;	snCfg.noiseTaps = 0x09;
	snCfg.negate = 1;	snCfg.stereo = 1;	snCfg.clkDiv = 8;
	snCfg.segaPSG = 0;
	
	retVal = SndEmu_Start(0x00, (DEV_GEN_CFG*)&snCfg, &snDefInf);
	if (retVal)
	{
		printf("SndEmu Start Error: %02X\n", retVal);
		goto Exit_AudDrvDeinit;
	}
	
	opts = AudioDrv_GetOptions(audDrv);
	opts->sampleRate = snDefInf.sampleRate;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	AudioDrv_SetCallback(audDrv, FillBuffer);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_SndDrvDeinit;
	}
	
	smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
	smplData[0] = (DEV_SMPL*)malloc(smplAlloc * sizeof(DEV_SMPL));
	smplData[1] = (DEV_SMPL*)malloc(smplAlloc * sizeof(DEV_SMPL));
	
	snDefInf.Reset(snDefInf.dataPtr);
	snWrite = (DEVFUNC_WRITE_A8D8)snDefInf.rwFuncs.Write;
	snWrite(snDefInf.dataPtr, 1, 0xDE);
	for (curReg = 0x8; curReg < 0xE; curReg += 0x2)
	{
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x0F);
		snWrite(snDefInf.dataPtr, 0, 0x3F);
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x10);
	}
	snWrite(snDefInf.dataPtr, 0, 0xE0 | 0x00);
	snWrite(snDefInf.dataPtr, 0, 0xE0 | 0x10);
	
	getchar();
	
	retVal = AudioDrv_Stop(audDrv);
	free(smplData[0]);	smplData[0] = NULL;
	free(smplData[1]);	smplData[1] = NULL;
	
Exit_SndDrvDeinit:
	SndEmu_Stop(&snDefInf);
Exit_AudDrvDeinit:
	AudioDrv_Deinit(&audDrv);
Exit_AudDeinit:
	Audio_Deinit();
	printf("Done.\n");
	
#if _DEBUG
	if (_CrtDumpMemoryLeaks())
		_getch();
#endif
	
	return 0;
}

static UINT32 FillBuffer(void* Params, UINT32 bufSize, void* data)
{
	UINT32 smplCount;
	INT16* SmplPtr16;
	UINT32 curSmpl;
	
	smplCount = bufSize / smplSize;
	snDefInf.Update(snDefInf.dataPtr, smplCount, smplData);
	switch(smplSize)
	{
	case 4:
		SmplPtr16 = (INT16*)data;
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, SmplPtr16 += 2)
		{
			SmplPtr16[0] = smplData[0][curSmpl];
			SmplPtr16[1] = smplData[1][curSmpl];
		}
		break;
	default:
		curSmpl = 0;
		break;
	}
	
	return curSmpl * smplSize;
}

static void SetupDirectSound(void* audDrv)
{
#ifdef _WIN32
	void* aDrv;
	HWND hWndConsole;
	
	aDrv = AudioDrv_GetDrvData(audDrv);
#if _WIN32_WINNT >= 0x500
	hWndConsole = GetConsoleWindow();
#else
	hWndConsole = GetDesktopWindow();	// not as nice, but works
#endif
	DSound_SetHWnd(aDrv, hWndConsole);
#endif	// _WIN32
	
	return;
}
