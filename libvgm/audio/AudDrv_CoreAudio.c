// Audio Stream - Core Audio
//	- Core Audio

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <AudioToolbox/AudioQueue.h>

#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)

#include "../stdtype.h"

#include "AudioStream.h"
#include "../utils/OSMutex.h"


typedef struct _coreaudio_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	AudioStreamBasicDescription waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT32 bufNum;
	
	OS_MUTEX* hMutex;
	AudioQueueRef audioQueue;
	AudioQueueBufferRef *buffers;

	void* userParam;
	AUDFUNC_FILLBUF FillBuffer;
} DRV_CA;


UINT8 CoreAudio_IsAvailable(void);
UINT8 CoreAudio_Init(void);
UINT8 CoreAudio_Deinit(void);
const AUDIO_DEV_LIST* CoreAudio_GetDeviceList(void);
AUDIO_OPTS* CoreAudio_GetDefaultOpts(void);

UINT8 CoreAudio_Create(void** retDrvObj);
UINT8 CoreAudio_Destroy(void* drvObj);
UINT8 CoreAudio_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 CoreAudio_Stop(void* drvObj);
UINT8 CoreAudio_Pause(void* drvObj);
UINT8 CoreAudio_Resume(void* drvObj);

UINT8 CoreAudio_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 CoreAudio_GetBufferSize(void* drvObj);
UINT8 CoreAudio_IsBusy(void* drvObj);
UINT8 CoreAudio_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 CoreAudio_GetLatency(void* drvObj);

AUDIO_DRV audDrv_CA =
{
	{ADRVTYPE_OUT, ADRVSIG_CA, "Core Audio"},
	
	CoreAudio_IsAvailable,
	CoreAudio_Init, CoreAudio_Deinit,
	CoreAudio_GetDeviceList, CoreAudio_GetDefaultOpts,
	
	CoreAudio_Create, CoreAudio_Destroy,
	CoreAudio_Start, CoreAudio_Stop,
	CoreAudio_Pause, CoreAudio_Resume,
	
	CoreAudio_SetCallback, CoreAudio_GetBufferSize,
	CoreAudio_IsBusy, CoreAudio_WriteData,
	
	CoreAudio_GetLatency,
};

static char* caDevNames[1] = {"default"};
static UINT32 caDevice;
static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

static void CoreAudio_RenderOutputBuffer(void *userData, AudioQueueRef queue, AudioQueueBufferRef buffer);

UINT8 CoreAudio_IsAvailable(void)
{
	return 1;
}

UINT8 CoreAudio_Init(void)
{
	// TODO: generate a list of all Core Audio devices
	//UINT32 numDevs;
	//UINT32 curDev;
	//UINT32 devLstID;
	
	if (isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = caDevNames;
	
	
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

UINT8 CoreAudio_Deinit(void)
{
	//UINT32 curDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* CoreAudio_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* CoreAudio_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 CoreAudio_Create(void** retDrvObj)
{
	DRV_CA* drv;
	UINT8 retVal8;
	
	drv = (DRV_CA*)malloc(sizeof(DRV_CA));
	drv->devState = 0;
	drv->audioQueue = NULL;
	drv->buffers = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;

	drv->bufNum = 0;

	activeDrivers ++;
	retVal8 = OSMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		CoreAudio_Destroy(drv);
		*retDrvObj = NULL;
		return AERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 CoreAudio_Destroy(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	
	if (drv->devState != 0)
		CoreAudio_Stop(drvObj);
	if (drv->hMutex != NULL)
		OSMutex_Deinit(drv->hMutex);
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 CoreAudio_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	UINT64 tempInt64;
	int retVal;
	UINT8 retVal8;
	OSStatus res;
	UINT32 i;
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.mSampleRate = options->sampleRate;
	drv->waveFmt.mFormatID = kAudioFormatLinearPCM;
	drv->waveFmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian;
	drv->waveFmt.mBytesPerFrame = (options->numBitsPerSmpl / 8) * options->numChannels;
	drv->waveFmt.mFramesPerPacket = 1;
	drv->waveFmt.mBytesPerPacket = drv->waveFmt.mBytesPerFrame * drv->waveFmt.mFramesPerPacket;
	drv->waveFmt.mChannelsPerFrame = options->numChannels;
	drv->waveFmt.mBitsPerChannel = options->numBitsPerSmpl;

	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.mBytesPerPacket * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;

	
	res = AudioQueueNewOutput(&drv->waveFmt, CoreAudio_RenderOutputBuffer, drv, NULL, NULL, 0, &drv->audioQueue);
	if (res || drv->audioQueue == NULL)
		return AERR_API_ERR;
	
	drv->buffers = (AudioQueueBufferRef*) calloc(drv->bufCount, sizeof(AudioQueueBufferRef));

	for (i = 0; i < drv->bufCount; i++) {
		res = AudioQueueAllocateBuffer(drv->audioQueue, drv->bufSize, drv->buffers + i);
		if (res || drv->buffers[i] == NULL) {
			res = AudioQueueDispose(drv->audioQueue, true);
			drv->audioQueue = NULL;
			return 0xC0;	// open() failed
		}
		drv->buffers[i]->mAudioDataByteSize = drv->bufSize;
		// Prime the buffer allocated
		memset(drv->buffers[i]->mAudioData, 0, drv->buffers[i]->mAudioDataByteSize);
		AudioQueueEnqueueBuffer(drv->audioQueue, drv->buffers[i], 0, NULL);
	}

	res = AudioQueueStart(drv->audioQueue, NULL);
	if (res) {
		res = AudioQueueDispose(drv->audioQueue, true);
		drv->audioQueue = NULL;
		return AERR_API_ERR;
	}

	drv->devState = 1;
	
	return AERR_OK;
}

UINT8 CoreAudio_Stop(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	OSStatus res;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	free(drv->buffers);	drv->buffers = NULL;
	
	res = AudioQueueDispose(drv->audioQueue, true);
	if (res)
		return 0xC4;		// close failed -- but why ???
	drv->audioQueue = NULL;
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 CoreAudio_Pause(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	OSStatus res;
	
	if (drv->audioQueue == NULL)
		return 0xFF;

	res = AudioQueuePause(drv->audioQueue);
	if (res)
		return 0xFF;
	
	return AERR_OK;
}

UINT8 CoreAudio_Resume(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	OSStatus res;
	
	if (drv->audioQueue == NULL)
		return 0xFF;

	res = AudioQueueStart(drv->audioQueue, NULL);
	if (res)
		return 0xFF;
	
	return AERR_OK;
}


UINT8 CoreAudio_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	
	OSMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	OSMutex_Unlock(drv->hMutex);
	
	return AERR_OK;
}

UINT32 CoreAudio_GetBufferSize(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	
	return drv->bufSize;
}

UINT8 CoreAudio_IsBusy(void* drvObj)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	//return AERR_BUSY;
	return AERR_OK;
}

UINT8 CoreAudio_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_CA* drv = (DRV_CA*)drvObj;
	OSStatus res;
	UINT32 bufNum;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;

	OSMutex_Lock(drv->hMutex);

	bufNum = drv->bufNum;
	drv->bufNum = (bufNum + 1 >= drv->bufCount) ? 0 : bufNum + 1;

	memcpy(drv->buffers[bufNum]->mAudioData, data, dataSize);

	OSMutex_Unlock(drv->hMutex);

	res = AudioQueueEnqueueBuffer(drv->audioQueue, drv->buffers[bufNum], 0, NULL);	
	if (res)
		return 0xFF;
	return AERR_OK;
}


UINT32 CoreAudio_GetLatency(void* drvObj)
{
	//DRV_AO* drv = (DRV_AO*)drvObj;


	return 0;	// There's no API call that lets you receive the current latency.
}

// The callback that does the actual audio feeding
void CoreAudio_RenderOutputBuffer(void *userData, AudioQueueRef queue, AudioQueueBufferRef buffer) {
	DRV_CA *drv = (DRV_CA *)userData;

	OSMutex_Lock(drv->hMutex);
	if (drv->FillBuffer == NULL) {
		memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
	}
	else {
		drv->FillBuffer(drv, drv->userParam, buffer->mAudioDataByteSize, buffer->mAudioData);
	}
	OSMutex_Unlock(drv->hMutex);

	AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}
