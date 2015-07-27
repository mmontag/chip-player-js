// Audio Stream - DirectSound
// Required libraries:
//	- dsound.lib
//	- uuid.lib (for GUID_NULL)
//	- kernel32.lib (threads)
#define _CRTDBG_MAP_ALLOC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIRECTSOUND_VERSION	0x0600
#include <dsound.h>
#include <mmsystem.h>	// for WAVEFORMATEX

#include <stdtype.h>

#include "AudioStream.h"

#define EXT_C	extern "C"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

typedef struct _dsound_device
{
	GUID devGUID;
	char* devName;
	char* devModule;
} DSND_DEVICE;

typedef struct _directsound_driver
{
	void* audDrvPtr;
	UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufSegSize;
	UINT32 bufSegCount;
	UINT8* bufSpace;
	
	HWND hWnd;
	LPDIRECTSOUND dSndIntf;
	LPDIRECTSOUNDBUFFER dSndBuf;
	HANDLE hThread;
	DWORD idThread;
	AUDFUNC_FILLBUF FillBuffer;
	
	UINT32 writePos;
} DRV_DSND;


EXT_C UINT8 DSound_IsAvailable(void);
EXT_C UINT8 DSound_Init(void);
EXT_C UINT8 DSound_Deinit(void);
EXT_C const AUDIO_DEV_LIST* DSound_GetDeviceList(void);
EXT_C AUDIO_OPTS* DSound_GetDefaultOpts(void);

EXT_C UINT8 DSound_Create(void** retDrvObj);
EXT_C UINT8 DSound_Destroy(void* drvObj);
EXT_C UINT8 DSound_SetHWnd(void* drvObj, HWND hWnd);
EXT_C UINT8 DSound_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
EXT_C UINT8 DSound_Stop(void* drvObj);
EXT_C UINT8 DSound_Pause(void* drvObj);
EXT_C UINT8 DSound_Resume(void* drvObj);

EXT_C UINT8 DSound_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback);
static UINT32 GetFreeBytes(DRV_DSND* drv);
EXT_C UINT8 DSound_IsBusy(void* drvObj);
EXT_C UINT8 DSound_WriteData(void* drvObj, UINT32 dataSize, void* data);

EXT_C UINT32 DSound_GetLatency(void* drvObj);
static DWORD WINAPI DirectSoundThread(void* Arg);
static UINT8 WriteBuffer(DRV_DSND* drv, UINT32 dataSize, void* data);
static UINT8 ClearBuffer(DRV_DSND* drv);


// Notes:
//	- MS Visual C++ (6.0/2010) needs 'extern "C"' for variables
//	- GCC doesn't need it for variables and complains about it
//	  if written in the form 'extern "C" AUDIO_DRV ...'
//	- MS Visual C++ 6.0 complains if the prototype of a static function has
//	  'extern "C"', but the definition lacks it.
extern "C"
{
AUDIO_DRV audDrv_DSound =
{
	{ADRVTYPE_OUT, ADRVSIG_DSOUND, "DirectSound"},
	
	DSound_IsAvailable,
	DSound_Init, DSound_Deinit,
	DSound_GetDeviceList, DSound_GetDefaultOpts,
	
	DSound_Create, DSound_Destroy,
	DSound_Start, DSound_Stop,
	DSound_Pause, DSound_Resume,
	
	DSound_SetCallback,
	DSound_IsBusy, DSound_WriteData,
	
	DSound_GetLatency,
};
}	// extern "C"


static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

static UINT32 devLstAlloc;
static UINT32 devLstCount;
static DSND_DEVICE* devList;

UINT8 DSound_IsAvailable(void)
{
	return 1;
}

static BOOL CALLBACK DSEnumCallback(LPGUID lpGuid, LPCSTR lpcstrDescription,
									LPCSTR lpcstrModule, LPVOID lpContext)
{
	DSND_DEVICE* tempDev;
	
	if (devLstCount >= devLstAlloc)
	{
		devLstAlloc += 0x10;
		devList = (DSND_DEVICE*)realloc(devList, devLstAlloc * sizeof(DSND_DEVICE));
	}
	
	tempDev = &devList[devLstCount];
	if (lpGuid == NULL)
		memcpy(&tempDev->devGUID, &GUID_NULL, sizeof(GUID));
	else
		memcpy(&tempDev->devGUID, lpGuid, sizeof(GUID));
	tempDev->devName = strdup(lpcstrDescription);
	tempDev->devModule = strdup(lpcstrModule);
	devLstCount ++;
	
	return TRUE;
}

UINT8 DSound_Init(void)
{
	HRESULT retVal;
	UINT32 curDev;
	
	if (isInit)
		return AERR_WASDONE;
	
	devLstAlloc = 0;
	devLstCount = 0;
	devList = NULL;
	retVal = DirectSoundEnumerateA(&DSEnumCallback, NULL);
	if (retVal != DS_OK)
		return AERR_API_ERR;
	
	deviceList.devNames = (char**)malloc(sizeof(char*) * devLstCount);
	for (curDev = 0; curDev < devLstCount; curDev ++)
		deviceList.devNames[curDev] = devList[curDev].devName;
	deviceList.devCount = curDev;
	
	
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

UINT8 DSound_Deinit(void)
{
	UINT32 curDev;
	DSND_DEVICE* tempDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	
	for (curDev = 0; curDev < devLstCount; curDev ++)
	{
		tempDev = &devList[curDev];
		free(tempDev->devName);
		free(tempDev->devModule);
	}
	devLstCount = 0;
	devLstAlloc = 0;
	free(devList);	devList = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* DSound_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* DSound_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 DSound_Create(void** retDrvObj)
{
	DRV_DSND* drv;
	
	drv = (DRV_DSND*)malloc(sizeof(DRV_DSND));
	drv->devState = 0;
	drv->hWnd = NULL;
	drv->dSndIntf = NULL;
	drv->dSndBuf = NULL;
	drv->hThread = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 DSound_Destroy(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	if (drv->devState != 0)
		DSound_Stop(drvObj);
	if (drv->hThread != NULL)
	{
		TerminateThread(drv->hThread, 0);
		drv->hThread = NULL;
	}
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 DSound_SetHWnd(void* drvObj, HWND hWnd)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	drv->hWnd = hWnd;
	
	return AERR_OK;
}

UINT8 DSound_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	GUID* devGUID;
	UINT64 tempInt64;
	DSBUFFERDESC bufDesc;
	HRESULT retVal;
#ifdef NDEBUG
	BOOL retValB;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	if (deviceID >= devLstCount)
		return AERR_INVALID_DEV;
	if (drv->hWnd == NULL)
		return AERR_CALL_SPC_FUNC;
	
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
	drv->bufSegSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufSegCount = options->numBuffers ? options->numBuffers : 10;
	drv->bufSize = drv->bufSegSize * drv->bufSegCount;
	
	if (! memcmp(&devList[deviceID].devGUID, &GUID_NULL, sizeof(GUID)))
		devGUID = NULL;	// default device
	else
		devGUID = &devList[deviceID].devGUID;
	retVal = DirectSoundCreate(devGUID, &drv->dSndIntf, NULL);
	if (retVal != DS_OK)
		return AERR_API_ERR;
	
	retVal = drv->dSndIntf->SetCooperativeLevel(drv->hWnd, DSSCL_PRIORITY);
	if (retVal == DSERR_INVALIDPARAM)
		return AERR_CALL_SPC_FUNC;
	if (retVal != DS_OK)
		return AERR_API_ERR;
	
	// Create Secondary Sound Buffer
	bufDesc.dwSize = sizeof(DSBUFFERDESC);
	bufDesc.dwFlags = DSBCAPS_GLOBALFOCUS;	// Global Focus -> don't mute sound when losing focus
	bufDesc.dwBufferBytes = drv->bufSize;
	bufDesc.dwReserved = 0;
	bufDesc.lpwfxFormat = &drv->waveFmt;
	retVal = drv->dSndIntf->CreateSoundBuffer(&bufDesc, &drv->dSndBuf, NULL);
	if (retVal != DS_OK)
		return AERR_API_ERR;
	
	drv->hThread = CreateThread(NULL, 0, &DirectSoundThread, drv, CREATE_SUSPENDED, &drv->idThread);
	if (drv->hThread == NULL)
		return 0xC8;	// CreateThread failed
#ifdef NDEBUG
	retValB = SetThreadPriority(drv->hThread, THREAD_PRIORITY_TIME_CRITICAL);
	if (! retValB)
	{
		// Error setting priority
		// Try a lower priority, because too low priorities cause sound stuttering.
		retValB = SetThreadPriority(drv->hThread, THREAD_PRIORITY_HIGHEST);
	}
#endif
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSegSize);
	
	drv->writePos = 0;
	ClearBuffer(drv);
	retVal = drv->dSndBuf->Play(0, 0, DSBPLAY_LOOPING);
	
	drv->devState = 1;
	ResumeThread(drv->hThread);
	
	return AERR_OK;
}

UINT8 DSound_Stop(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	DWORD retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	if (drv->dSndBuf != NULL)
		drv->dSndBuf->Stop();
	
	retVal = WaitForSingleObject(drv->hThread, 100);
	if (retVal == WAIT_TIMEOUT)
		TerminateThread(drv->hThread, 0);
	CloseHandle(drv->hThread);	drv->hThread = NULL;
	
	free(drv->bufSpace);
	drv->bufSpace = NULL;
	if (drv->dSndBuf != NULL)
	{
		drv->dSndBuf->Release();
		drv->dSndBuf = NULL;
	}
	drv->dSndIntf->Release();
	drv->dSndIntf = NULL;
	
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 DSound_Pause(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->dSndBuf->Stop();
	return (retVal == DS_OK) ? AERR_OK : 0xFF;
}

UINT8 DSound_Resume(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->dSndBuf->Play(0, 0, DSBPLAY_LOOPING);
	return (retVal == DS_OK) ? AERR_OK : 0xFF;
}


UINT8 DSound_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	drv->FillBuffer = FillBufCallback;
	
	return AERR_OK;
}

static UINT32 GetFreeBytes(DRV_DSND* drv)
{
	HRESULT retVal;
	DWORD writeEndPos;
	
	retVal = drv->dSndBuf->GetCurrentPosition(NULL, &writeEndPos);
	if (retVal != DS_OK)
		return 0;
	
	if (drv->writePos <= writeEndPos)
		return writeEndPos - drv->writePos;
	else
		return (drv->bufSize - drv->writePos) + writeEndPos;
}

UINT8 DSound_IsBusy(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	UINT32 freeBytes;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	freeBytes = GetFreeBytes(drv);
	return (freeBytes >= drv->bufSegSize) ? AERR_OK : AERR_BUSY;
}

UINT8 DSound_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	UINT32 freeBytes;
	
	if (dataSize > drv->bufSegSize)
		return AERR_TOO_MUCH_DATA;
	
	freeBytes = GetFreeBytes(drv);
	while(freeBytes < dataSize)
		Sleep(1);
	
	WriteBuffer(drv, dataSize, data);
	return AERR_OK;
}


UINT32 DSound_GetLatency(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	UINT32 bytesBehind;
#if 0
	DWORD readPos;
	HRESULT retVal;
	
	retVal = drv->dSndBuf->GetCurrentPosition(&readPos, NULL);
	if (retVal != DS_OK)
		return 0;
	
	if (drv->writePos >= readPos)
		bytesBehind = drv->writePos - readPos;
	else
		bytesBehind = (drv->bufSize - readPos) + drv->writePos;
#else
	bytesBehind = drv->bufSize - GetFreeBytes(drv);
#endif
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static DWORD WINAPI DirectSoundThread(void* Arg)
{
	DRV_DSND* drv = (DRV_DSND*)Arg;
	UINT32 wrtBytes;
	UINT32 didBuffers;	// number of processed buffers
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		
		while(GetFreeBytes(drv) >= drv->bufSegSize && drv->FillBuffer != NULL)
		{
			wrtBytes = drv->FillBuffer(drv->audDrvPtr, drv->bufSegSize, drv->bufSpace);
			WriteBuffer(drv, wrtBytes, drv->bufSpace);
			didBuffers ++;
		}
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
		//while(drv->PauseThread && drv->devState == 1)
		//	Sleep(1);
	}
	
	return 0;
}

static UINT8 WriteBuffer(DRV_DSND* drv, UINT32 dataSize, void* data)
{
	HRESULT retVal;
	DWORD bufSize1;
	void* bufPtr1;
	DWORD bufSize2;
	void* bufPtr2;
	
	retVal = drv->dSndBuf->Lock(drv->writePos, dataSize, &bufPtr1, &bufSize1,
								&bufPtr2, &bufSize2, 0x00);
	if (retVal != DS_OK)
		return 0xFF;
	
	memcpy(bufPtr1, data, bufSize1);
	drv->writePos += bufSize1;
	if (drv->writePos >= drv->bufSize)
		drv->writePos -= drv->bufSize;
	if (bufPtr2 != NULL)
	{
		memcpy(bufPtr2, (char*)data + bufSize1, bufSize2);
		drv->writePos += bufSize2;
	}
	
	retVal = drv->dSndBuf->Unlock(bufPtr1, bufSize1, bufPtr2, bufSize2);
	if (retVal != DS_OK)
		return 0xFF;
	
	return AERR_OK;
}

static UINT8 ClearBuffer(DRV_DSND* drv)
{
	HRESULT retVal;
	DWORD bufSize;
	void* bufPtr;
	
	retVal = drv->dSndBuf->Lock(0x00, 0x00, &bufPtr, &bufSize,
								NULL, NULL, DSBLOCK_ENTIREBUFFER);
	if (retVal != DS_OK)
		return 0xFF;
	
	memset(bufPtr, 0x00, bufSize);
	
	retVal = drv->dSndBuf->Unlock(bufPtr, bufSize, NULL, 0);
	if (retVal != DS_OK)
		return 0xFF;
	
	return AERR_OK;
}
