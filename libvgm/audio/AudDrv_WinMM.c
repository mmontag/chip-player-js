// Audio Stream - Windows Multimedia API WinMM
//	- winmm.lib
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include "../stdtype.h"

#include "AudioStream.h"
#include "../utils/OSThread.h"
#include "../utils/OSSignal.h"
#include "../utils/OSMutex.h"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

#ifdef NEED_WAVEFMT
#pragma pack(1)
typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	UINT16 cbSize;
} WAVEFORMATEX;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001
#endif


typedef struct _winmm_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	OS_THREAD* hThread;
	OS_SIGNAL* hSignal;
	OS_MUTEX* hMutex;
	HWAVEOUT hWaveOut;
	WAVEHDR* waveHdrs;
	void* userParam;
	AUDFUNC_FILLBUF FillBuffer;
	
	UINT32 BlocksSent;
	UINT32 BlocksPlayed;
} DRV_WINMM;


UINT8 WinMM_IsAvailable(void);
UINT8 WinMM_Init(void);
UINT8 WinMM_Deinit(void);
const AUDIO_DEV_LIST* WinMM_GetDeviceList(void);
AUDIO_OPTS* WinMM_GetDefaultOpts(void);

UINT8 WinMM_Create(void** retDrvObj);
UINT8 WinMM_Destroy(void* drvObj);
UINT8 WinMM_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 WinMM_Stop(void* drvObj);
UINT8 WinMM_Pause(void* drvObj);
UINT8 WinMM_Resume(void* drvObj);

UINT8 WinMM_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 WinMM_GetBufferSize(void* drvObj);
UINT8 WinMM_IsBusy(void* drvObj);
UINT8 WinMM_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 WinMM_GetLatency(void* drvObj);
static void WaveOutThread(void* Arg);
static void WriteBuffer(DRV_WINMM* drv, WAVEHDR* wHdr);
static void BufCheck(DRV_WINMM* drv);


AUDIO_DRV audDrv_WinMM =
{
	{ADRVTYPE_OUT, ADRVSIG_WINMM, "WinMM"},
	
	WinMM_IsAvailable,
	WinMM_Init, WinMM_Deinit,
	WinMM_GetDeviceList, WinMM_GetDefaultOpts,
	
	WinMM_Create, WinMM_Destroy,
	WinMM_Start, WinMM_Stop,
	WinMM_Pause, WinMM_Resume,
	
	WinMM_SetCallback, WinMM_GetBufferSize,
	WinMM_IsBusy, WinMM_WriteData,
	
	WinMM_GetLatency,
};


static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;
static UINT* devListIDs;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 WinMM_IsAvailable(void)
{
	UINT32 numDevs;
	
	numDevs = waveOutGetNumDevs();
	return numDevs ? 1 : 0;
}

UINT8 WinMM_Init(void)
{
	UINT numDevs;
	WAVEOUTCAPSA woCaps;
	UINT32 curDev;
	UINT32 devLstID;
	MMRESULT retValMM;
	
	if (isInit)
		return AERR_WASDONE;
	
	numDevs = waveOutGetNumDevs();
	deviceList.devCount = 1 + numDevs;
	deviceList.devNames = (char**)malloc(deviceList.devCount * sizeof(char*));
	devListIDs = (UINT*)malloc(deviceList.devCount * sizeof(UINT));
	devLstID = 0;
	
	retValMM = waveOutGetDevCapsA(WAVE_MAPPER, &woCaps, sizeof(WAVEOUTCAPSA));
	if (retValMM == MMSYSERR_NOERROR)
	{
		devListIDs[devLstID] = WAVE_MAPPER;
		deviceList.devNames[devLstID] = strdup(woCaps.szPname);
		devLstID ++;
	}
	for (curDev = 0; curDev < numDevs; curDev ++)
	{
		retValMM = waveOutGetDevCapsA(curDev, &woCaps, sizeof(WAVEOUTCAPSA));
		if (retValMM == MMSYSERR_NOERROR)
		{
			devListIDs[devLstID] = curDev;
			deviceList.devNames[devLstID] = strdup(woCaps.szPname);
			devLstID ++;
		}
	}
	deviceList.devCount = devLstID;
	
	
	memset(&defOptions, 0x00, sizeof(AUDIO_OPTS));
	defOptions.sampleRate = 44100;
	defOptions.numChannels = 2;
	defOptions.numBitsPerSmpl = 16;
	defOptions.usecPerBuf = 10000;	// 10 ms per buffer
	defOptions.numBuffers = 10;	// 100 ms latency
	
	
	activeDrivers = 0;
	isInit = 1;
	
	return AERR_OK;
}

UINT8 WinMM_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
		free(deviceList.devNames[curDev]);
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	free(devListIDs);			devListIDs = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* WinMM_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* WinMM_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 WinMM_Create(void** retDrvObj)
{
	DRV_WINMM* drv;
	UINT8 retVal8;
	
	drv = (DRV_WINMM*)malloc(sizeof(DRV_WINMM));
	drv->devState = 0;
	drv->hWaveOut = NULL;
	drv->hThread = NULL;
	drv->hSignal = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	retVal8  = OSSignal_Init(&drv->hSignal, 0);
	retVal8 |= OSMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		WinMM_Destroy(drv);
		*retDrvObj = NULL;
		return AERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 WinMM_Destroy(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	if (drv->devState != 0)
		WinMM_Stop(drvObj);
	if (drv->hThread != NULL)
	{
		OSThread_Cancel(drv->hThread);
		OSThread_Deinit(drv->hThread);
	}
	if (drv->hSignal != NULL)
		OSSignal_Deinit(drv->hSignal);
	if (drv->hMutex != NULL)
		OSMutex_Deinit(drv->hMutex);
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 WinMM_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT64 tempInt64;
	UINT32 woDevID;
	UINT32 curBuf;
	WAVEHDR* tempWavHdr;
	MMRESULT retValMM;
	UINT8 retVal8;
#ifdef NDEBUG
	HANDLE hWinThr;
	BOOL retValB;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	if (deviceID >= deviceList.devCount)
		return AERR_INVALID_DEV;
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	drv->waveFmt.cbSize = 0;
	
	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	woDevID = devListIDs[deviceID];
	retValMM = waveOutOpen(&drv->hWaveOut, woDevID, &drv->waveFmt, 0x00, 0x00, CALLBACK_NULL);
	if (retValMM != MMSYSERR_NOERROR)
		return 0xC0;		// waveOutOpen failed
	
	OSSignal_Reset(drv->hSignal);
	retVal8 = OSThread_Init(&drv->hThread, &WaveOutThread, drv);
	if (retVal8)
	{
		retValMM = waveOutClose(drv->hWaveOut);
		drv->hWaveOut = NULL;
		return 0xC8;	// CreateThread failed
	}
#ifdef NDEBUG
	hWinThr = *(HANDLE*)OSThread_GetHandle(drv->hThread);
	retValB = SetThreadPriority(hWinThr, THREAD_PRIORITY_TIME_CRITICAL);
	if (! retValB)
	{
		// Error setting priority
		// Try a lower priority, because too low priorities cause sound stuttering.
		retValB = SetThreadPriority(hWinThr, THREAD_PRIORITY_HIGHEST);
	}
#endif
	
	drv->waveHdrs = (WAVEHDR*)malloc(drv->bufCount * sizeof(WAVEHDR));
	drv->bufSpace = (UINT8*)malloc(drv->bufCount * drv->bufSize);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		tempWavHdr = &drv->waveHdrs[curBuf];
		tempWavHdr->lpData = (LPSTR)&drv->bufSpace[drv->bufSize * curBuf];
		tempWavHdr->dwBufferLength = drv->bufSize;
		tempWavHdr->dwBytesRecorded = 0x00;
		tempWavHdr->dwUser = 0x00;
		tempWavHdr->dwFlags = 0x00;
		tempWavHdr->dwLoops = 0x00;
		tempWavHdr->lpNext = NULL;
		tempWavHdr->reserved = 0x00;
		retValMM = waveOutPrepareHeader(drv->hWaveOut, tempWavHdr, sizeof(WAVEHDR));
		tempWavHdr->dwFlags |= WHDR_DONE;
	}
	drv->BlocksSent = 0;
	drv->BlocksPlayed = 0;
	
	drv->devState = 1;
	OSSignal_Signal(drv->hSignal);
	
	return AERR_OK;
}

UINT8 WinMM_Stop(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
	MMRESULT retValMM;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	OSThread_Join(drv->hThread);
	OSThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	retValMM = waveOutReset(drv->hWaveOut);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		retValMM = waveOutUnprepareHeader(drv->hWaveOut, &drv->waveHdrs[curBuf], sizeof(WAVEHDR));
	free(drv->bufSpace);	drv->bufSpace = NULL;
	free(drv->waveHdrs);	drv->waveHdrs = NULL;
	
	retValMM = waveOutClose(drv->hWaveOut);
	if (retValMM != MMSYSERR_NOERROR)
		return 0xC4;		// waveOutClose failed -- but why ???
	drv->hWaveOut = NULL;
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 WinMM_Pause(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutPause(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? AERR_OK : 0xFF;
}

UINT8 WinMM_Resume(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutRestart(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? AERR_OK : 0xFF;
}


UINT8 WinMM_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	OSMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	OSMutex_Unlock(drv->hMutex);
	
	return AERR_OK;
}

UINT32 WinMM_GetBufferSize(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	return drv->bufSize;
}

UINT8 WinMM_IsBusy(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		if (drv->waveHdrs[curBuf].dwFlags & WHDR_DONE)
			return AERR_OK;
	}
	return AERR_BUSY;
}

UINT8 WinMM_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
	WAVEHDR* tempWavHdr;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;
	
	OSMutex_Lock(drv->hMutex);
	while(1)
	{
		for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		{
			tempWavHdr = &drv->waveHdrs[curBuf];
			if (tempWavHdr->dwFlags & WHDR_DONE)
			{
				memcpy(tempWavHdr->lpData, data, dataSize);
				tempWavHdr->dwBufferLength = dataSize;
				
				WriteBuffer(drv, tempWavHdr);
				OSMutex_Unlock(drv->hMutex);
				return AERR_OK;
			}
		}
	}
	OSMutex_Unlock(drv->hMutex);
	return AERR_BUSY;
}


UINT32 WinMM_GetLatency(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 bufBehind;
	UINT32 smplsBehind;
	
	BufCheck(drv);
	bufBehind = drv->BlocksSent - drv->BlocksPlayed;
	smplsBehind = drv->bufSmpls * bufBehind;
	return smplsBehind * 1000 / drv->waveFmt.nSamplesPerSec;
}

static void WaveOutThread(void* Arg)
{
	DRV_WINMM* drv = (DRV_WINMM*)Arg;
	UINT32 curBuf;
	UINT32 didBuffers;	// number of processed buffers
	WAVEHDR* tempWavHdr;
	
	OSSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		OSMutex_Lock(drv->hMutex);
		for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		{
			tempWavHdr = &drv->waveHdrs[curBuf];
			if (tempWavHdr->dwFlags & WHDR_DONE)
			{
				if (drv->FillBuffer == NULL)
					break;
				tempWavHdr->dwBufferLength = drv->FillBuffer(drv->audDrvPtr, drv->userParam,
															drv->bufSize, tempWavHdr->lpData);
				WriteBuffer(drv, tempWavHdr);
				didBuffers ++;
			}
			if (drv->devState > 1)
				break;
		}
		OSMutex_Unlock(drv->hMutex);
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
		//while(drv->PauseThread && drv->devState == 1)
		//	Sleep(1);
	}
	
	return;
}

static void WriteBuffer(DRV_WINMM* drv, WAVEHDR* wHdr)
{
	if (wHdr->dwUser & 0x01)
		drv->BlocksPlayed ++;
	else
		wHdr->dwUser |= 0x01;
	
	waveOutWrite(drv->hWaveOut, wHdr, sizeof(WAVEHDR));
	drv->BlocksSent ++;
	
	return;
}


static void BufCheck(DRV_WINMM* drv)
{
	UINT32 curBuf;
	
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		if (drv->waveHdrs[curBuf].dwFlags & WHDR_DONE)
		{
			if (drv->waveHdrs[curBuf].dwUser & 0x01)
			{
				drv->waveHdrs[curBuf].dwUser &= ~0x01;
				drv->BlocksPlayed ++;
			}
		}
	}
	
	return;
}
