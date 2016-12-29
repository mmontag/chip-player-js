#ifdef _WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifdef _WIN32
int __cdecl _getch(void);	// from conio.h
#else
#define _getch	getchar
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include <common_def.h>
#include "audio/AudioStream.h"
#include "audio/AudioStream_SpcDrvFuns.h"
#include "emu/EmuStructs.h"
#include "emu/SoundEmu.h"
#include "emu/Resampler.h"
#include "emu/SoundDevs.h"
#include "emu/EmuCores.h"
#include "emu/cores/sn764intf.h"	// for SN76496_CFG
#include "emu/cores/segapcm.h"		// for SEGAPCM_CFG


typedef struct _vgm_file_header
{
	UINT32 fccVGM;
	UINT32 lngEOFOffset;
	UINT32 lngVersion;
	UINT32 lngHzPSG;
	UINT32 lngHzYM2413;
	UINT32 lngGD3Offset;
	UINT32 lngTotalSamples;
	UINT32 lngLoopOffset;
	UINT32 lngLoopSamples;
	UINT32 lngRate;
	UINT16 shtPSG_Feedback;
	UINT8 bytPSG_SRWidth;
	UINT8 bytPSG_Flags;
	UINT32 lngHzYM2612;
	UINT32 lngHzYM2151;
	UINT32 lngDataOffset;
	UINT32 lngHzSPCM;
	UINT32 lngSPCMIntf;
	UINT32 lngHzRF5C68;
	UINT32 lngHzYM2203;
	UINT32 lngHzYM2608;
	UINT32 lngHzYM2610;
	UINT32 lngHzYM3812;
	UINT32 lngHzYM3526;
	UINT32 lngHzY8950;
	UINT32 lngHzYMF262;
	UINT32 lngHzYMF278B;
	UINT32 lngHzYMF271;
	UINT32 lngHzYMZ280B;
	UINT32 lngHzRF5C164;
	UINT32 lngHzPWM;
	UINT32 lngHzAY8910;
	UINT8 bytAYType;
	UINT8 bytAYFlag;
	UINT8 bytAYFlagYM2203;
	UINT8 bytAYFlagYM2608;
	UINT8 bytVolumeModifier;
	UINT8 bytReserved2;
	INT8 bytLoopBase;
	UINT8 bytLoopModifier;
	UINT32 lngHzGBDMG;
	UINT32 lngHzNESAPU;
	UINT32 lngHzMultiPCM;
	UINT32 lngHzUPD7759;
	UINT32 lngHzOKIM6258;
	UINT8 bytOKI6258Flags;
	UINT8 bytK054539Flags;
	UINT8 bytC140Type;
	UINT8 bytReservedFlags;
	UINT32 lngHzOKIM6295;
	UINT32 lngHzK051649;
	UINT32 lngHzK054539;
	UINT32 lngHzHuC6280;
	UINT32 lngHzC140;
	UINT32 lngHzK053260;
	UINT32 lngHzPokey;
	UINT32 lngHzQSound;
	UINT32 lngHzSCSP;
	UINT32 lngExtraOffset;
	UINT32 lngHzWSwan;
	UINT32 lngHzVSU;
	UINT32 lngHzSAA1099;
	UINT32 lngHzES5503;
	UINT32 lngHzES5506;
	UINT8 bytES5503Chns;
	UINT8 bytES5506Chns;
	UINT8 bytC352ClkDiv;
	UINT8 bytESReserved;
	UINT32 lngHzX1_010;
	UINT32 lngHzC352;
	UINT32 lngHzGA20;
} VGM_HEADER;
typedef struct
{
	DEV_INFO defInf;
	RESMPL_STATE resmpl;
	DEVFUNC_WRITE_A8D8 write8;		// write 8-bit data to 8-bit register/offset
	DEVFUNC_WRITE_A16D8 writeM8;	// write 8-bit data to 16-bit memory offset
	DEVFUNC_WRITE_MEMSIZE romSize;
	DEVFUNC_WRITE_BLOCK romWrite;
	DEVFUNC_WRITE_MEMSIZE romSizeB;
	DEVFUNC_WRITE_BLOCK romWriteB;
} VGM_CHIPDEV;


int main(int argc, char* argv[]);
static UINT32 FillBuffer(void* Params, UINT32 bufSize, void* Data);
static void SetupDirectSound(void* audDrv);
static void InitVGMChips(void);
static void DeinitVGMChips(void);
static void SendChipCommand_Data8(UINT8 chipID, UINT8 chipNum, UINT8 ofs, UINT8 data);
static void SendChipCommand_RegData8(UINT8 chipID, UINT8 chipNum, UINT8 port, UINT8 reg, UINT8 data);
static void SendChipCommand_MemData8(UINT8 chipID, UINT8 chipNum, UINT16 ofs, UINT8 data);
static UINT32 DoVgmCommand(UINT8 cmd, const UINT8* data);
static void ReadVGMFile(UINT32 samples);


static UINT32 smplSize;
static void* audDrv;
static DEV_INFO snDefInf;
static UINT32 smplAlloc;
static WAVE_32BS* smplData;
static volatile bool canRender;

static UINT32 VGMLen;
static UINT8* VGMData;
static UINT32 VGMPos;
static VGM_HEADER VGMHdr;
static UINT32 VGMSmplPos;
static UINT32 renderSmplPos;
#define CHIP_COUNT	0x29
static VGM_CHIPDEV VGMChips[CHIP_COUNT];
static UINT32 sampleRate;

int main(int argc, char* argv[])
{
	gzFile hFile;
	UINT8 retVal;
	UINT32 drvCount;
	UINT32 idWavOut;
	UINT32 idWavOutDev;
	AUDDRV_INFO* drvInfo;
	AUDIO_OPTS* opts;
	UINT32 tempData[2];
	
	if (argc < 2)
	{
		printf("Usage: vgmtest vgmfile.vgz\n");
		return 0;
	}
	
	printf("Loading VGM ...\n");
	hFile = gzopen(argv[1], "rb");
	if (hFile == NULL)
	{
		printf("Error opening file.\n");
		return 1;
	}
	VGMLen = 0;
	VGMData = NULL;
	gzread(hFile, tempData, 0x08);
	if (! memcmp(&tempData[0], "Vgm ", 4))
	{
		VGMLen = 0x04 + tempData[0];
		VGMData = (UINT8*)malloc(VGMLen);
		memcpy(&VGMData[0x00], tempData, 0x08);
		gzread(hFile, &VGMData[0x08], VGMLen - 0x08);
		
		memcpy(&tempData[0], &VGMData[0x34], 0x04);
		tempData[0] += (tempData[0] == 0) ? 0x40 : 0x34;
		tempData[1] = sizeof(VGM_HEADER);
		if (tempData[0] > tempData[1])
			tempData[0] = tempData[1];
		memset(&VGMHdr, 0x00, tempData[1]);
		memcpy(&VGMHdr, &VGMData[0x00], tempData[0]);
	}
	gzclose(hFile);
	if (! VGMLen)
	{
		printf("Error reading file!\n");
		return 2;
	}
	
	printf("Opening Audio Device ...\n");
	Audio_Init();
	drvCount = Audio_GetDriverCount();
	if (! drvCount)
		goto Exit_AudDeinit;
	
	idWavOut = 1;
	idWavOutDev = 0;
	
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
	
	sampleRate = 44100;
	
	opts = AudioDrv_GetOptions(audDrv);
	//opts->sampleRate = snDefInf.sampleRate;
	//opts->sampleRate = 96000;
	opts->numChannels = 2;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	
	InitVGMChips();
	
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
	smplData = (WAVE_32BS*)malloc(smplAlloc * sizeof(WAVE_32BS));
	
	canRender = true;
	getchar();
	canRender = false;
	
	retVal = AudioDrv_Stop(audDrv);
	free(smplData);	smplData = NULL;
	
Exit_SndDrvDeinit:
	DeinitVGMChips();
	free(VGMData);	VGMData = NULL;
//Exit_AudDrvDeinit:
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
	WAVE_32BS fnlSmpl;
	UINT8 curChip;
	
	if (! canRender)
	{
		memset(data, 0x00, bufSize);
		return bufSize;
	}
	
	smplCount = bufSize / smplSize;
	if (smplCount > smplAlloc)
		smplCount = smplAlloc;
	memset(smplData, 0, smplCount * sizeof(WAVE_32BS));
	
	ReadVGMFile(smplCount);
	// I know that using a for-loop has a bad performance, but it's just for testing anyway.
	for (curChip = 0x00; curChip < CHIP_COUNT; curChip ++)
	{
		if (VGMChips[curChip].defInf.dataPtr != NULL)
			Resmpl_Execute(&VGMChips[curChip].resmpl, smplCount, smplData);
	}
	
	SmplPtr16 = (INT16*)data;
	for (curSmpl = 0; curSmpl < smplCount; curSmpl ++, SmplPtr16 += 2)
	{
		fnlSmpl.L = smplData[curSmpl].L >> 8;
		fnlSmpl.R = smplData[curSmpl].R >> 8;
		if (fnlSmpl.L < -0x8000)
			fnlSmpl.L = -0x8000;
		else if (fnlSmpl.L > +0x7FFF)
			fnlSmpl.L = +0x7FFF;
		if (fnlSmpl.R < -0x8000)
			fnlSmpl.R = -0x8000;
		else if (fnlSmpl.R > +0x7FFF)
			fnlSmpl.R = +0x7FFF;
		SmplPtr16[0] = (INT16)fnlSmpl.L;
		SmplPtr16[1] = (INT16)fnlSmpl.R;
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


static void InitVGMChips(void)
{
	static UINT32 chipOfs[CHIP_COUNT] =
	{	0x0C, 0x10, 0x2C, 0x30, 0x38, 0x40, 0x44, 0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x74,
		0x80, 0x84, 0x88, 0x8C, 0x90, 0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8,
		0xC0, 0xC4, 0xC8, 0xCC, 0xD0, 0xD8, 0xDC, 0xE0};
	UINT8* vgmHdrArr;
	UINT8 curChip;
	UINT32 chpClk;
	DEV_GEN_CFG devCfg;
	VGM_CHIPDEV* cDev;
	UINT8 retVal;
	
	memset(&VGMChips, 0x00, sizeof(VGMChips));
	vgmHdrArr = (UINT8*)&VGMHdr;	// can't use VGMData due to the data offset
	for (curChip = 0x00; curChip < CHIP_COUNT; curChip ++)
	{
		memcpy(&chpClk, &vgmHdrArr[chipOfs[curChip]], 0x04);
		if (! chpClk)
			continue;
		cDev = &VGMChips[curChip];
		
		devCfg.emuCore = 0x00;
		devCfg.srMode = DEVRI_SRMODE_NATIVE;
		devCfg.clock = chpClk & ~0x40000000;
		devCfg.smplRate = sampleRate;
		switch(curChip)
		{
		case DEVID_SN76496:
			{
				SN76496_CFG snCfg;
				
				if (! VGMHdr.bytPSG_SRWidth)
					VGMHdr.bytPSG_SRWidth = 0x10;
				if (! VGMHdr.shtPSG_Feedback)
					VGMHdr.shtPSG_Feedback = 0x09;
				//if (! VGMHdr.bytPSG_Flags)
				//	VGMHdr.bytPSG_Flags = 0x00;
				devCfg.emuCore = FCC_MAXM;
				snCfg._genCfg = devCfg;
				snCfg.shiftRegWidth = VGMHdr.bytPSG_SRWidth;
				snCfg.noiseTaps = (UINT8)VGMHdr.shtPSG_Feedback;
				snCfg.segaPSG = (VGMHdr.bytPSG_Flags & 0x01) ? 0 : 1;
				snCfg.negate = (VGMHdr.bytPSG_Flags & 0x02) ? 1 : 0;
				snCfg.stereo = (VGMHdr.bytPSG_Flags & 0x04) ? 0 : 1;
				snCfg.clkDiv = (VGMHdr.bytPSG_Flags & 0x08) ? 1 : 8;
				
				retVal = SndEmu_Start(curChip, (DEV_GEN_CFG*)&snCfg, &cDev->defInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write8);
			}
			break;
		case DEVID_SEGAPCM:
			{
				SEGAPCM_CFG spCfg;
				
				spCfg._genCfg = devCfg;
				spCfg.bnkshift = (VGMHdr.lngSPCMIntf >> 0) & 0xFF;
				spCfg.bnkmask = (VGMHdr.lngSPCMIntf >> 16) & 0xFF;
				retVal = SndEmu_Start(curChip, (DEV_GEN_CFG*)&spCfg, &cDev->defInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&cDev->writeM8);
				SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&cDev->romSize);
				SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&cDev->romWrite);
			}
			break;
		case DEVID_RF5C68:
		case 0x10:	// DEVID_RF5C164
			if (curChip == DEVID_RF5C68)
				devCfg.emuCore = FCC_MAME;
			else
				devCfg.emuCore = FCC_GENS;
			retVal = SndEmu_Start(DEVID_RF5C68, &devCfg, &cDev->defInf);
			if (retVal)
				break;
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write8);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, (void**)&cDev->writeM8);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&cDev->romWrite);
			break;
		case DEVID_YM2610:
			retVal = SndEmu_Start(curChip, &devCfg, &cDev->defInf);
			if (retVal)
				break;
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write8);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'A', (void**)&cDev->romSize);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'A', (void**)&cDev->romWrite);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'B', (void**)&cDev->romSizeB);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'B', (void**)&cDev->romWriteB);
			break;
		case DEVID_YMF278B:
			retVal = SndEmu_Start(curChip, &devCfg, &cDev->defInf);
			if (retVal)
				break;
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write8);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x524F, (void**)&cDev->romSize);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x524F, (void**)&cDev->romWrite);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x5241, (void**)&cDev->romWriteB);
			if (cDev->romWrite != NULL)
			{
				const char* romFile = "yrw801.rom";
				FILE* hFile;
				UINT32 yrwSize;
				UINT8* yrwData;
				
				hFile = fopen(romFile, "rb");
				if (hFile == NULL)
				{
					printf("Warning: Couldn't load %s!\n", romFile);
					break;
				}
				
				fseek(hFile, 0, SEEK_END);
				yrwSize = ftell(hFile);
				rewind(hFile);
				yrwData = (UINT8*)malloc(yrwSize);
				fread(yrwData, 1, yrwSize, hFile);
				fclose(hFile);
				
				cDev->romSize(cDev->defInf.dataPtr, yrwSize);
				cDev->romWrite(cDev->defInf.dataPtr, 0x00, yrwSize, yrwData);
				free(yrwData);
			}
			break;
		default:
			if (curChip == DEVID_YM2612)
				devCfg.emuCore = FCC_GPGX;
			else if (curChip == DEVID_YM3812)
				devCfg.emuCore = FCC_ADLE;
			else if (curChip == DEVID_YMF262)
				devCfg.emuCore = FCC_ADLE;
			retVal = SndEmu_Start(curChip, &devCfg, &cDev->defInf);
			if (retVal)
				break;
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write8);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&cDev->romSize);
			SndEmu_GetDeviceFunc(cDev->defInf.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&cDev->romWrite);
			break;
		}
		if (retVal)
		{
			cDev->defInf.dataPtr = NULL;
			cDev->defInf.devDef = NULL;
			continue;
		}
		// already done by SndEmu_Start()
		//cDev->defInf.devDef->Reset(cDev->defInf.dataPtr);
		
		Resmpl_SetVals(&cDev->resmpl, 0xFF, 0x100, sampleRate);
		Resmpl_DevConnect(&cDev->resmpl, &cDev->defInf);
		Resmpl_Init(&cDev->resmpl);
	}
	VGMSmplPos = 0;
	renderSmplPos = 0;
	VGMPos = VGMHdr.lngDataOffset;
	VGMPos += VGMPos ? 0x34 : 0x40;
	
	return;
}

static void DeinitVGMChips(void)
{
	UINT8 curChip;
	VGM_CHIPDEV* cDev;
	
	for (curChip = 0x00; curChip < CHIP_COUNT; curChip ++)
	{
		cDev = &VGMChips[curChip];
		if (cDev->defInf.dataPtr == NULL)
			continue;
		
		Resmpl_Deinit(&cDev->resmpl);
		SndEmu_Stop(&cDev->defInf);
	}
	
	return;
}

static void SendChipCommand_Data8(UINT8 chipID, UINT8 chipNum, UINT8 ofs, UINT8 data)
{
	VGM_CHIPDEV* cDev;
	
	cDev = &VGMChips[chipID];
	if (cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->defInf.dataPtr, ofs, data);
	return;
}

static void SendChipCommand_RegData8(UINT8 chipID, UINT8 chipNum, UINT8 port, UINT8 reg, UINT8 data)
{
	VGM_CHIPDEV* cDev;
	
	cDev = &VGMChips[chipID];
	if (cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->defInf.dataPtr, (port << 1) | 0, reg);
	cDev->write8(cDev->defInf.dataPtr, (port << 1) | 1, data);
	return;
}

static void SendChipCommand_MemData8(UINT8 chipID, UINT8 chipNum, UINT16 ofs, UINT8 data)
{
	VGM_CHIPDEV* cDev;
	
	cDev = &VGMChips[chipID];
	if (cDev->writeM8 == NULL)
		return;
	
	cDev->writeM8(cDev->defInf.dataPtr, ofs, data);
	return;
}

static void WriteChipROM(UINT8 chipID, UINT8 chipNum, UINT8 memID,
						 UINT32 memSize, UINT32 dataOfs, UINT32 dataSize, const UINT8* data)
{
	VGM_CHIPDEV* cDev;
	
	cDev = &VGMChips[chipID];
	if (memID == 0)
	{
		if (cDev->romSize != NULL)
			cDev->romSize(cDev->defInf.dataPtr, memSize);
		if (cDev->romWrite != NULL)
			cDev->romWrite(cDev->defInf.dataPtr, dataOfs, dataSize, data);
	}
	else
	{
		if (cDev->romSizeB != NULL)
			cDev->romSizeB(cDev->defInf.dataPtr, memSize);
		if (cDev->romWriteB != NULL)
			cDev->romWriteB(cDev->defInf.dataPtr, dataOfs, dataSize, data);
	}
	return;
}

static void WriteChipRAM(UINT8 chipID, UINT8 chipNum,
						 UINT32 dataOfs, UINT32 dataSize, const UINT8* data)
{
	VGM_CHIPDEV* cDev;
	
	cDev = &VGMChips[chipID];
	if (cDev->romWrite != NULL)
		cDev->romWrite(cDev->defInf.dataPtr, dataOfs, dataSize, data);
	return;
}

static const UINT8 VGM_CMDLEN[0x10] =
{	0x01, 0x01, 0x01, 0x02, 0x03, 0x03, 0x00, 0x01,
	0x01, 0x00, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05};
typedef struct
{
	UINT8 chipID;
	UINT8 memIdx;
} VGM_ROMDUMP_IDS;
static const VGM_ROMDUMP_IDS VGMROM_CHIPS[0x14] =
{	0x04, 0x00,	// SegaPCM
	0x07, 0x00,	// YM2608 DeltaT
	0x08, 0x00,	// YM2610 ADPCM
	0x08, 0x01,	// YM2610 DeltaT
	0x0D, 0x00,	// YMF278B ROM
	0x0E, 0x00,	// YMF271
	0x0F, 0x00,	// YMZ280B
	0x0D, 0x01,	// YMF278B RAM
	0x0B, 0x00,	// Y8950 DeltaT
	0x15, 0x00,	// MultiPCM
	0x16, 0x00,	// uPD7759
	0x18, 0x00,	// OKIM6295
	0x1A, 0x00,	// K054539
	0x1C, 0x00,	// C140
	0x1D, 0x00,	// K053260
	0x1F, 0x00,	// QSound
	0x25, 0x00,	// ES5506
	0x26, 0x00,	// X1-010
	0x27, 0x00,	// C352
	0x28, 0x00,	// GA20
};
static const VGM_ROMDUMP_IDS VGMRAM_CHIPS1[0x03] =
{	0x05, 0x00,	// RF5C68
	0x10, 0x00,	// RF5C164
	0x14, 0x00,	// NES APU
};
static const VGM_ROMDUMP_IDS VGMRAM_CHIPS2[0x02] =
{	0x20, 0x00,	// SCSP
	0x24, 0x00,	// ES5503
};

static UINT32 DoVgmCommand(UINT8 cmd, const UINT8* data)
{
	UINT8 chipID;
	
	if (cmd >= 0x70 && cmd <= 0x7F)
	{
		VGMSmplPos += 1 + (cmd & 0x0F);
		return 0x01;
	}
	else if (cmd >= 0x80 && cmd <= 0x8F)
	{
		VGMSmplPos += (cmd & 0x0F);
		return 0x01;
	}
	
	chipID = 0;
	if (cmd == 0x30)
	{
		chipID = 1;
		cmd += 0x20;
	}
	else if (cmd == 0x3F)
	{
		chipID = 1;
		cmd += 0x10;
	}
	else if (cmd >= 0xA1 && cmd <= 0xAF)
	{
		chipID = 1;
		cmd -= 0x50;
	}
	switch(cmd)
	{
	case 0x61:
		VGMSmplPos += (data[0x01] << 0) | (data[0x02] << 8);
		return 0x03;
	case 0x62:
		VGMSmplPos += 735;
		return 0x01;
	case 0x63:
		VGMSmplPos += 882;
		return 0x01;
	case 0x66:
		return 0x00;	// terminate
	case 0x67:
		{
			UINT32 dblkLen;
			UINT8 dblkType;
			UINT32 memSize;
			UINT32 dataOfs;
			UINT32 dataSize;
			
			dblkType = data[0x02];
			memcpy(&dblkLen, &data[0x03], 0x04);
			chipID = (dblkLen & 0x80000000) >> 31;
			dblkLen &= 0x7FFFFFFF;
			
			switch(dblkType & 0xC0)
			{
			case 0x80:	// ROM/RAM write
				dblkType &= 0x3F;
				if (dblkType >= 0x14)
					break;
				memcpy(&memSize, &data[0x07], 0x04);
				memcpy(&dataOfs, &data[0x0B], 0x04);
				dataSize = dblkLen - 0x08;
				WriteChipROM(VGMROM_CHIPS[dblkType].chipID, chipID, VGMROM_CHIPS[dblkType].memIdx,
							memSize, dataOfs, dataSize, &data[0x0F]);
				break;
			case 0xC0:	// RAM Write
				dblkType &= 0x3F;
				if (! (dblkType & 0x20))
				{
					dataOfs = 0x00;
					memcpy(&dataOfs, &data[0x07], 0x02);
					dataSize = dblkLen - 0x02;
					WriteChipRAM(VGMRAM_CHIPS1[dblkType].chipID, chipID,
								dataOfs, dataSize, &data[0x09]);
				}
				else
				{
					memcpy(&dataOfs, &data[0x07], 0x04);
					dataSize = dblkLen - 0x04;
					WriteChipRAM(VGMRAM_CHIPS2[dblkType].chipID, chipID,
								dataOfs, dataSize, &data[0x0B]);
				}
				break;
			}
			return 0x07 + dblkLen;
		}
	case 0x68:
		return 0x0C;
	case 0x50:	// SN76489
		SendChipCommand_Data8(0x00, chipID, 0, data[0x01]);
		return 0x02;
	case 0x51:	// YM2413
		SendChipCommand_RegData8(0x01, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x52:	// YM2612
	case 0x53:
		SendChipCommand_RegData8(0x02, chipID, cmd & 0x01, data[0x01], data[0x02]);
		return 0x03;
	case 0x54:	// YM2151
		SendChipCommand_RegData8(0x03, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x55:	// YM2203
		SendChipCommand_RegData8(0x06, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x56:	// YM2608
	case 0x57:
		SendChipCommand_RegData8(0x07, chipID, cmd & 0x01, data[0x01], data[0x02]);
		return 0x03;
	case 0x58:	// YM2610
	case 0x59:
		SendChipCommand_RegData8(0x08, chipID, cmd & 0x01, data[0x01], data[0x02]);
		return 0x03;
	case 0x5A:	// YM3812
		SendChipCommand_RegData8(0x09, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x5B:	// YM3526
		SendChipCommand_RegData8(0x0A, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x5C:	// Y8950
		SendChipCommand_RegData8(0x0B, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x5D:	// YMZ280B
		SendChipCommand_RegData8(0x0F, chipID, 0, data[0x01], data[0x02]);
		return 0x03;
	case 0x5E:	// YMF262
	case 0x5F:
		SendChipCommand_RegData8(0x0C, chipID, cmd & 0x01, data[0x01], data[0x02]);
		return 0x03;
	case 0xB0:	// RF5C68
		SendChipCommand_Data8(0x05, chipID, data[0x01], data[0x02]);
		return 0x03;
	case 0xB1:	// RF5C164
		SendChipCommand_Data8(0x10, chipID, data[0x01], data[0x02]);
		return 0x03;
	case 0xB8:	// OKIM6295
		chipID = (data[0x01] & 0x80) >> 7;
		SendChipCommand_Data8(0x18, chipID, data[0x01] & 0x7F, data[0x02]);
		return 0x03;
	case 0xC0:	// Sega PCM memory write
		{
			UINT16 memOfs;
			
			memcpy(&memOfs, &data[0x01], 0x02);
			chipID = (data[0x01] & 0x8000) >> 15;
			memOfs &= 0x7FFF;
			SendChipCommand_MemData8(0x04, chipID, memOfs, data[0x03]);
		}
		return 0x04;
	case 0xC1:	// RF5C68 memory write
	case 0xC2:	// RF5C164 memory write
		{
			UINT16 memOfs;
			memcpy(&memOfs, &data[0x01], 0x02);
			if (cmd == 0xC1)
				SendChipCommand_MemData8(0x05, chipID, memOfs, data[0x03]);
			else
				SendChipCommand_MemData8(0x10, chipID, memOfs, data[0x03]);
		}
		return 0x04;
	case 0xD0:	// YMF278B
		chipID = (data[0x01] & 0x80) >> 7;
		SendChipCommand_RegData8(0x0D, chipID, data[0x01] & 0x7F, data[0x02], data[0x03]);
		return 0x04;
	case 0xD1:	// YMF271
		chipID = (data[0x01] & 0x80) >> 7;
		SendChipCommand_RegData8(0x0E, chipID, data[0x01] & 0x7F, data[0x02], data[0x03]);
		return 0x04;
	default:
		return VGM_CMDLEN[cmd >> 4];
	}
}

static void ReadVGMFile(UINT32 samples)
{
	UINT32 cmdLen;
	
	renderSmplPos += samples;
	while(VGMSmplPos <= renderSmplPos)
	{
		cmdLen = DoVgmCommand(VGMData[VGMPos], &VGMData[VGMPos]);
		if (! cmdLen)
			break;
		
		VGMPos += cmdLen;
	}
	
	return;
}
