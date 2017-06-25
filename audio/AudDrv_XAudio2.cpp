// Audio Stream - XAudio2
// Required libraries:
//	- kernel32.lib (threads)
//	- ole32.lib (COM stuff)
#define _CRTDBG_MAP_ALLOC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
// suppress warnings about 'uuid' attribute directive and MSVC pragmas
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#include <xaudio2.h>
#include <mmsystem.h>	// for WAVEFORMATEX

#include <stdtype.h>

#include "AudioStream.h"

#define EXT_C	extern "C"


typedef struct _xaudio2_driver
{
	void* audDrvPtr;
	UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	IXAudio2* xAudIntf;
	IXAudio2MasteringVoice* xaMstVoice;
	IXAudio2SourceVoice* xaSrcVoice;
	XAUDIO2_BUFFER* xaBufs;
	HANDLE hThread;
	DWORD idThread;
	AUDFUNC_FILLBUF FillBuffer;
	
	UINT32 writeBuf;
} DRV_XAUD2;


EXT_C UINT8 XAudio2_IsAvailable(void);
EXT_C UINT8 XAudio2_Init(void);
EXT_C UINT8 XAudio2_Deinit(void);
EXT_C const AUDIO_DEV_LIST* XAudio2_GetDeviceList(void);
EXT_C AUDIO_OPTS* XAudio2_GetDefaultOpts(void);

EXT_C UINT8 XAudio2_Create(void** retDrvObj);
EXT_C UINT8 XAudio2_Destroy(void* drvObj);
EXT_C UINT8 XAudio2_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
EXT_C UINT8 XAudio2_Stop(void* drvObj);
EXT_C UINT8 XAudio2_Pause(void* drvObj);
EXT_C UINT8 XAudio2_Resume(void* drvObj);

EXT_C UINT8 XAudio2_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback);
EXT_C UINT32 XAudio2_GetBufferSize(void* drvObj);
EXT_C UINT8 XAudio2_IsBusy(void* drvObj);
EXT_C UINT8 XAudio2_WriteData(void* drvObj, UINT32 dataSize, void* data);

EXT_C UINT32 XAudio2_GetLatency(void* drvObj);
static DWORD WINAPI XAudio2Thread(void* Arg);


extern "C"
{
AUDIO_DRV audDrv_XAudio2 =
{
	{ADRVTYPE_OUT, ADRVSIG_XAUD2, "XAudio2"},
	
	XAudio2_IsAvailable,
	XAudio2_Init, XAudio2_Deinit,
	XAudio2_GetDeviceList, XAudio2_GetDefaultOpts,
	
	XAudio2_Create, XAudio2_Destroy,
	XAudio2_Start, XAudio2_Stop,
	XAudio2_Pause, XAudio2_Resume,
	
	XAudio2_SetCallback, XAudio2_GetBufferSize,
	XAudio2_IsBusy, XAudio2_WriteData,
	
	XAudio2_GetLatency,
};
}	// extern "C"

static DWORD WINAPI XAudio2Thread(void* Arg);


static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;
static UINT32* devListIDs;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 XAudio2_IsAvailable(void)
{
	return 1;
}

UINT8 XAudio2_Init(void)
{
	HRESULT retVal;
	UINT32 curDev;
	UINT32 devLstID;
	size_t devNameSize;
	IXAudio2* xAudIntf;
	XAUDIO2_DEVICE_DETAILS xDevData;
	
	if (isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	devListIDs = NULL;
	
	retVal = CoInitialize(NULL);
	if (! (retVal == S_OK || retVal == S_FALSE))
		return AERR_API_ERR;
	
	retVal = XAudio2Create(&xAudIntf, 0x00, XAUDIO2_DEFAULT_PROCESSOR);
	if (retVal != S_OK)
	{
		CoUninitialize();
		return AERR_API_ERR;
	}
	
	retVal = xAudIntf->GetDeviceCount(&deviceList.devCount);
	if (retVal != S_OK)
	{
		CoUninitialize();
		return AERR_API_ERR;
	}
	deviceList.devNames = (char**)malloc(deviceList.devCount * sizeof(char*));
	devListIDs = (UINT32*)malloc(deviceList.devCount * sizeof(UINT32));
	devLstID = 0;
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
	{
		retVal = xAudIntf->GetDeviceDetails(curDev, &xDevData);
		if (retVal == S_OK)
		{
			devListIDs[devLstID] = curDev;
			devNameSize = wcslen(xDevData.DisplayName) + 1;
			deviceList.devNames[devLstID] = (char*)malloc(devNameSize);
			wcstombs(deviceList.devNames[devLstID], xDevData.DisplayName, devNameSize);
			devLstID ++;
		}
	}
	deviceList.devCount = devLstID;
	xAudIntf->Release();	xAudIntf = NULL;
	
	
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

UINT8 XAudio2_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
		free(deviceList.devNames[curDev]);
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	free(devListIDs);			devListIDs = NULL;
	
	CoUninitialize();
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* XAudio2_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* XAudio2_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 XAudio2_Create(void** retDrvObj)
{
	DRV_XAUD2* drv;
	
	drv = (DRV_XAUD2*)malloc(sizeof(DRV_XAUD2));
	drv->devState = 0;
	drv->xAudIntf = NULL;
	drv->xaMstVoice = NULL;
	drv->xaSrcVoice = NULL;
	drv->xaBufs = NULL;
	drv->hThread = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 XAudio2_Destroy(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	if (drv->devState != 0)
		XAudio2_Stop(drvObj);
	if (drv->hThread != NULL)
	{
		TerminateThread(drv->hThread, 0);
		drv->hThread = NULL;
	}
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 XAudio2_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	UINT64 tempInt64;
	UINT32 curBuf;
	XAUDIO2_BUFFER* tempXABuf;
	HRESULT retVal;
#ifdef NDEBUG
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
	
	retVal = CoInitialize(NULL);	// call again, in case Init() was called by another thread
	if (! (retVal == S_OK || retVal == S_FALSE))
		return AERR_API_ERR;
	
	retVal = XAudio2Create(&drv->xAudIntf, 0x00, XAUDIO2_DEFAULT_PROCESSOR);
	if (retVal != S_OK)
		return AERR_API_ERR;
	
	retVal = drv->xAudIntf->CreateMasteringVoice(&drv->xaMstVoice,
				XAUDIO2_DEFAULT_CHANNELS, drv->waveFmt.nSamplesPerSec, 0x00,
				devListIDs[deviceID], NULL);
	if (retVal != S_OK)
		return AERR_API_ERR;
	
	retVal = drv->xAudIntf->CreateSourceVoice(&drv->xaSrcVoice,
				&drv->waveFmt, 0x00, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
	
	drv->hThread = CreateThread(NULL, 0, &XAudio2Thread, drv, CREATE_SUSPENDED, &drv->idThread);
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
	
	drv->xaBufs = (XAUDIO2_BUFFER*)calloc(drv->bufCount, sizeof(XAUDIO2_BUFFER));
	drv->bufSpace = (UINT8*)malloc(drv->bufCount * drv->bufSize);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		tempXABuf = &drv->xaBufs[curBuf];
		tempXABuf->AudioBytes = drv->bufSize;
		tempXABuf->pAudioData = &drv->bufSpace[drv->bufSize * curBuf];
		tempXABuf->pContext = NULL;
		tempXABuf->Flags = 0x00;
	}
	drv->writeBuf = 0;
	
	retVal = drv->xaSrcVoice->Start(0x00, XAUDIO2_COMMIT_NOW);
	
	drv->devState = 1;
	ResumeThread(drv->hThread);
	
	return AERR_OK;
}

UINT8 XAudio2_Stop(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	DWORD retValDW;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	if (drv->xaSrcVoice != NULL)
		retVal = drv->xaSrcVoice->Stop(0x00, XAUDIO2_COMMIT_NOW);
	
	retValDW = WaitForSingleObject(drv->hThread, 100);
	if (retValDW == WAIT_TIMEOUT)
		TerminateThread(drv->hThread, 0);
	CloseHandle(drv->hThread);	drv->hThread = NULL;
	
	drv->xaMstVoice->DestroyVoice();	drv->xaMstVoice = NULL;
	drv->xaSrcVoice->DestroyVoice();	drv->xaSrcVoice = NULL;
	drv->xAudIntf->Release();			drv->xAudIntf = NULL;
	
	free(drv->xaBufs);		drv->xaBufs = NULL;
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	CoUninitialize();
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 XAudio2_Pause(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->xaSrcVoice->Stop(0x00, XAUDIO2_COMMIT_NOW);
	return (retVal == S_OK) ? AERR_OK : 0xFF;
}

UINT8 XAudio2_Resume(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->xaSrcVoice->Start(0x00, XAUDIO2_COMMIT_NOW);
	return (retVal == S_OK) ? AERR_OK : 0xFF;
}


UINT8 XAudio2_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	drv->FillBuffer = FillBufCallback;
	
	return AERR_OK;
}

UINT32 XAudio2_GetBufferSize(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	return drv->bufSize;
}

UINT8 XAudio2_IsBusy(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_VOICE_STATE xaVocState;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	return (xaVocState.BuffersQueued < drv->bufCount) ? AERR_OK : AERR_BUSY;
}

UINT8 XAudio2_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_BUFFER* tempXABuf;
	XAUDIO2_VOICE_STATE xaVocState;
	HRESULT retVal;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	while(xaVocState.BuffersQueued >= drv->bufCount)
	{
		Sleep(1);
		drv->xaSrcVoice->GetState(&xaVocState);
	}
	
	tempXABuf = &drv->xaBufs[drv->writeBuf];
	memcpy((void*)tempXABuf->pAudioData, data, dataSize);
	tempXABuf->AudioBytes = dataSize;
	
	retVal = drv->xaSrcVoice->SubmitSourceBuffer(tempXABuf, NULL);
	drv->writeBuf ++;
	if (drv->writeBuf >= drv->bufCount)
		drv->writeBuf -= drv->bufCount;
	return AERR_OK;
}


UINT32 XAudio2_GetLatency(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_VOICE_STATE xaVocState;
	UINT32 bufBehind;
	UINT32 bytesBehind;
	UINT32 curBuf;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	
	bufBehind = xaVocState.BuffersQueued;
	curBuf = drv->writeBuf;
	bytesBehind = 0;
	while(bufBehind > 0)
	{
		if (curBuf == 0)
			curBuf = drv->bufCount;
		curBuf --;
		bytesBehind += drv->xaBufs[curBuf].AudioBytes;
		bufBehind --;
	}
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static DWORD WINAPI XAudio2Thread(void* Arg)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)Arg;
	XAUDIO2_VOICE_STATE xaVocState;
	XAUDIO2_BUFFER* tempXABuf;
	UINT32 didBuffers;	// number of processed buffers
	HRESULT retVal;
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		
		drv->xaSrcVoice->GetState(&xaVocState);
		while(xaVocState.BuffersQueued < drv->bufCount && drv->FillBuffer != NULL)
		{
			tempXABuf = &drv->xaBufs[drv->writeBuf];
			tempXABuf->AudioBytes = drv->FillBuffer(drv->audDrvPtr,
										drv->bufSize, (void*)tempXABuf->pAudioData);
			
			retVal = drv->xaSrcVoice->SubmitSourceBuffer(tempXABuf, NULL);
			drv->writeBuf ++;
			if (drv->writeBuf >= drv->bufCount)
				drv->writeBuf -= drv->bufCount;
			didBuffers ++;
			
			drv->xaSrcVoice->GetState(&xaVocState);
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
