#ifdef _WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>	// for memset()

#ifdef _WIN32
int __cdecl _getch(void);	// from conio.h
#else
#define _getch	getchar
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include "common_def.h"
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/EmuStructs.h"
#include "emu/SoundEmu.h"
#include "emu/Resampler.h"
#include "emu/SoundDevs.h"
#include "emu/EmuCores.h"
#include "emu/cores/sn764intf.h"	// for SN76496_CFG


int main(int argc, char* argv[]);
#ifdef AUDDRV_DSOUND
static void SetupDirectSound(void* audDrv);
#endif
static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* Data);


static UINT32 smplSize;
static void* audDrv;
static DEV_INFO snDefInf;
static DEV_INFO okiDefInf;
static DEV_INFO opllDefInf;
static RESMPL_STATE snResmpl;
static RESMPL_STATE okiResmpl;
static RESMPL_STATE opllResmpl;
static UINT32 smplAlloc;
static DEV_SMPL* smplData[2];
static volatile bool canRender;

static void OPLL_Write(DEVFUNC_WRITE_A8D8 write, void* info, UINT8 addr, UINT8 data)
{
	write(info, 0, addr);
	write(info, 1, data);
	return;
}

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
	DEVFUNC_WRITE_A8D8 snWrite;
	DEVFUNC_WRITE_A8D8 okiWrite;
	DEVFUNC_WRITE_A8D8 opllWrite;
	
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
#ifdef AUDDRV_DSOUND
	if (drvInfo->drvSig == ADRVSIG_DSOUND)
		SetupDirectSound(audDrv);
#endif
	
	
	// --- sound emulation setup ---
	// setup SN76496 (Maxim emulator)
	devCfg.emuCore = FCC_MAXM;
	devCfg.srMode = DEVRI_SRMODE_NATIVE;
	devCfg.flags = 0x00;
	devCfg.clock = 3579545;
	devCfg.smplRate = 48000;
	snCfg._genCfg = devCfg;
	snCfg.shiftRegWidth = 0x10;	snCfg.noiseTaps = 0x09;
	snCfg.negate = 1;	snCfg.stereo = 1;	snCfg.clkDiv = 8;
	snCfg.segaPSG = 0;
	snCfg.t6w28_tone = NULL;
	
	retVal = SndEmu_Start(DEVID_SN76496, (DEV_GEN_CFG*)&snCfg, &snDefInf);
	if (retVal)
	{
		printf("SndEmu Start Error: %02X\n", retVal);
		goto Exit_AudDrvDeinit;
	}
	
	// setup OKIM6295
	devCfg.emuCore = 0;	// default core
	devCfg.srMode = DEVRI_SRMODE_NATIVE;
	devCfg.flags = 0x00;
	devCfg.clock = 0x101D00;
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
			
			// load sample ROM to sound chip
			SndEmu_GetDeviceFunc(okiDefInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&okiRomSize);
			SndEmu_GetDeviceFunc(okiDefInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&okiRomWrite);
			okiRomSize(okiDefInf.dataPtr, fileLen);
			okiRomWrite(okiDefInf.dataPtr, 0x00, fileLen, fileData);
			free(fileData);
		}
	}
	
	// setup YM2413 (EMU2413)
	devCfg.emuCore = FCC_EMU_;
	devCfg.srMode = DEVRI_SRMODE_CUSTOM;	// TODO: DEVRI_SRMODE_NATIVE ?
	devCfg.flags = 0x00;
	devCfg.clock = 3579545;
	retVal = SndEmu_Start(DEVID_YM2413, &devCfg, &opllDefInf);
	
	
	// --- audio output setup ---
	opts = AudioDrv_GetOptions(audDrv);
	//opts->sampleRate = snDefInf.sampleRate;
	//opts->sampleRate = 96000;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	canRender = false;
	AudioDrv_SetCallback(audDrv, FillBuffer, NULL);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = AudioDrv_Start(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_SndDrvDeinit;
	}
	
	
	// setup resampler
	smplAlloc = AudioDrv_GetBufferSize(audDrv) / smplSize;
	smplData[0] = (DEV_SMPL*)malloc(smplAlloc * sizeof(DEV_SMPL) * 2);
	smplData[1] = &smplData[0][smplAlloc];
	
	snDefInf.devDef->Reset(snDefInf.dataPtr);
	okiDefInf.devDef->Reset(okiDefInf.dataPtr);
	opllDefInf.devDef->Reset(opllDefInf.dataPtr);
	
	Resmpl_SetVals(&snResmpl, 0xFF, 0xC0, opts->sampleRate);
	Resmpl_DevConnect(&snResmpl, &snDefInf);
	Resmpl_Init(&snResmpl);
	
	Resmpl_SetVals(&okiResmpl, 0xFF, 0x100, opts->sampleRate);
	Resmpl_DevConnect(&okiResmpl, &okiDefInf);
	Resmpl_Init(&okiResmpl);
	
	Resmpl_SetVals(&opllResmpl, 0xFF, 0x100, opts->sampleRate);
	Resmpl_DevConnect(&opllResmpl, &opllDefInf);
	Resmpl_Init(&opllResmpl);
	canRender = true;
	
	// get functions for executing sound chip register writes
	SndEmu_GetDeviceFunc(snDefInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&snWrite);
	SndEmu_GetDeviceFunc(okiDefInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&okiWrite);
	SndEmu_GetDeviceFunc(opllDefInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&opllWrite);
	
	// write data to the sound chip registers, so that they make sound
	snWrite(snDefInf.dataPtr, 0, 0x8B);	// PSG channel 1 frequency 0x06B (lower 4 bits)
	snWrite(snDefInf.dataPtr, 0, 0x06);	// frequency 0x06B (upper 6 bits)
	snWrite(snDefInf.dataPtr, 0, 0x93);	// PSG channel 1 volume 0x3
	OPLL_Write(opllWrite, opllDefInf.dataPtr, 0x30, 0x70);	// OPLL channel 0 instrument 7, volume 0
	OPLL_Write(opllWrite, opllDefInf.dataPtr, 0x10, 0x80);	// channel 0 frequency 0x180 (lower 8 bits)
	OPLL_Write(opllWrite, opllDefInf.dataPtr, 0x20, 0x17);	// channel 0 frequency 0x180 (upper 1 bit), octave 3, key on
	Sleep(250);
	okiWrite(okiDefInf.dataPtr, 0, 0x80 | 0x0D);	// OKI6295 sample 0D
	okiWrite(okiDefInf.dataPtr, 0, 0x10);	// channel 0 (mask 0x1), volume 0x0
	Sleep(400);
	okiWrite(okiDefInf.dataPtr, 0x0C, 1);	// set pin 7 = high
	okiWrite(okiDefInf.dataPtr, 0, 0x80 | 0x0D);	// sample 0D
	okiWrite(okiDefInf.dataPtr, 0, 0x20);	// channel 1 (mask 0x2), volume 0x0
	Sleep(1000);
	snWrite(snDefInf.dataPtr, 0, 0x99);
	
	getchar();
	
	retVal = AudioDrv_Stop(audDrv);
	Resmpl_Deinit(&snResmpl);
	Resmpl_Deinit(&okiResmpl);
	Resmpl_Deinit(&opllResmpl);
	free(smplData[0]);
	smplData[0] = NULL;	smplData[1] = NULL;
	
Exit_SndDrvDeinit:
	SndEmu_Stop(&snDefInf);
	SndEmu_Stop(&okiDefInf);
	SndEmu_Stop(&opllDefInf);
Exit_AudDrvDeinit:
	AudioDrv_Deinit(&audDrv);
Exit_AudDeinit:
	Audio_Deinit();
	printf("Done.\n");
	
#if defined(_DEBUG) && (_MSC_VER >= 1400)
	if (_CrtDumpMemoryLeaks())
		_getch();
#endif
	
	return 0;
}

static UINT32 FillBuffer(void* drvStruct, void* userParam, UINT32 bufSize, void* data)
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
	// emulate the sound chips
	// The resampler requests samples when needed and mixes everything into the smplDataW buffer.
	Resmpl_Execute(&snResmpl, smplCount, smplDataW);
	Resmpl_Execute(&okiResmpl, smplCount, smplDataW);
	Resmpl_Execute(&opllResmpl, smplCount, smplDataW);
	switch(smplSize)
	{
	case 4:
		SmplPtr16 = (INT16*)data;
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

#ifdef AUDDRV_DSOUND
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
#endif
