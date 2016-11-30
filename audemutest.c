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
static DEV_INFO okiDefInf;
static RESMPL_STATE snResmpl;
static RESMPL_STATE okiResmpl;
static UINT32 smplAlloc;
static DEV_SMPL* smplData[2];
static volatile bool canRender;

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
	DEVFUNC_WRITE_A8D8 okiWrite;
	//UINT8 curReg;
	
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
	
	retVal = SndEmu_Start(DEVID_SN76496, (DEV_GEN_CFG*)&snCfg, &snDefInf);
	if (retVal)
	{
		printf("SndEmu Start Error: %02X\n", retVal);
		goto Exit_AudDrvDeinit;
	}
	
	devCfg.emuCore = 0;
	devCfg.srMode = DEVRI_SRMODE_NATIVE;
	devCfg.clock = 0x80000000 | 0x101D00;
	retVal = SndEmu_Start(DEVID_OKIM6295, &devCfg, &okiDefInf);
	{
		FILE* hFile = fopen("122a05.bin", "rb");	// load Hexion sample ROM
		if (hFile != NULL)
		{
			UINT32 fileLen;
			UINT8* fileData;
			DEVFUNC_WRITE_MEMSIZE okiRomSize;
			DEVFUNC_WRITE_BLOCK okiRomWrite;
			
			fseek(hFile, 0, SEEK_END);
			fileLen = ftell(hFile);
			rewind(hFile);
			fileData = (UINT8*)malloc(fileLen);
			fread(fileData, 0x01, fileLen, hFile);
			fclose(hFile);
			
			SndEmu_GetDeviceFunc(&okiDefInf, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&okiRomSize);
			SndEmu_GetDeviceFunc(&okiDefInf, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&okiRomWrite);
			okiRomSize(okiDefInf.dataPtr, fileLen);
			okiRomWrite(okiDefInf.dataPtr, 0x00, fileLen, fileData);
			free(fileData);
		}
	}
	
	opts = AudioDrv_GetOptions(audDrv);
	//opts->sampleRate = snDefInf.sampleRate;
	//opts->sampleRate = 96000;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	canRender = false;
	AudioDrv_SetCallback(audDrv, FillBuffer);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_SndDrvDeinit;
	}
	
	smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
	smplData[0] = (DEV_SMPL*)malloc(smplAlloc * sizeof(DEV_SMPL) * 2);
	smplData[1] = &smplData[0][smplAlloc];
	
	snDefInf.Reset(snDefInf.dataPtr);
	okiDefInf.Reset(okiDefInf.dataPtr);
	
	snResmpl.ResampleMode = 0xFF;
	snResmpl.SmpRateSrc = snDefInf.sampleRate;
	snResmpl.SmpRateDst = opts->sampleRate;
	snResmpl.VolumeL = 0x100;	snResmpl.VolumeR = 0x100;
	snResmpl.StreamUpdate = snDefInf.Update;
	snResmpl.SU_DataPtr = snDefInf.dataPtr;
	SndEmu_ResamplerInit(&snResmpl);
	
	okiResmpl.ResampleMode = 0xFF;
	okiResmpl.SmpRateSrc = okiDefInf.sampleRate;
	okiResmpl.SmpRateDst = opts->sampleRate;
	okiResmpl.VolumeL = 0x100;	okiResmpl.VolumeR = 0x100;
	okiResmpl.StreamUpdate = okiDefInf.Update;
	okiResmpl.SU_DataPtr = okiDefInf.dataPtr;
	SndEmu_ResamplerInit(&okiResmpl);
	canRender = true;
	
	SndEmu_GetDeviceFunc(&snDefInf, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&snWrite);
	SndEmu_GetDeviceFunc(&okiDefInf, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&okiWrite);
	/*snWrite(snDefInf.dataPtr, 1, 0xDE);
	for (curReg = 0x8; curReg < 0xE; curReg += 0x2)
	{
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x0F);
		snWrite(snDefInf.dataPtr, 0, 0x3F);
		snWrite(snDefInf.dataPtr, 0, (curReg << 4) | 0x10);
	}
	snWrite(snDefInf.dataPtr, 0, 0xE0 | 0x00);
	snWrite(snDefInf.dataPtr, 0, 0xE0 | 0x10);*/
	snWrite(snDefInf.dataPtr, 0, 0x8B);
	snWrite(snDefInf.dataPtr, 0, 0x06);
	snWrite(snDefInf.dataPtr, 0, 0x93);
	okiWrite(okiDefInf.dataPtr, 0, 0x80 | 0x0D);	// sample 0D
	okiWrite(okiDefInf.dataPtr, 0, 0x10);	// channel 0 (mask 0x1), volume 0x0
	
	getchar();
	
	retVal = AudioDrv_Stop(audDrv);
	SndEmu_ResamplerDeinit(&snResmpl);
	SndEmu_ResamplerDeinit(&okiResmpl);
	free(smplData[0]);
	smplData[0] = NULL;	smplData[1] = NULL;
	
Exit_SndDrvDeinit:
	SndEmu_Stop(&snDefInf);
	SndEmu_Stop(&okiDefInf);
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
	WAVE_32BS* smplDataW = (WAVE_32BS*)smplData[0];
	
	if (! canRender)
	{
		memset(data, 0x00, bufSize);
		return bufSize;
	}
	
	smplCount = bufSize / smplSize;
	memset(smplData[0], 0, bufSize);
	memset(smplData[1], 0, bufSize);
	//snDefInf.Update(snDefInf.dataPtr, smplCount, smplData);
	SndEmu_ResamplerExecute(&snResmpl, smplCount, smplDataW);
	SndEmu_ResamplerExecute(&okiResmpl, smplCount, smplDataW);
	switch(smplSize)
	{
	case 4:
		SmplPtr16 = (INT16*)data;
		/*for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, SmplPtr16 += 2)
		{
			SmplPtr16[0] = smplData[0][curSmpl];
			SmplPtr16[1] = smplData[1][curSmpl];
		}*/
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, SmplPtr16 += 2)
		{
			SmplPtr16[0] = smplDataW[curSmpl].L >> 8;
			SmplPtr16[1] = smplDataW[curSmpl].R >> 8;
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
