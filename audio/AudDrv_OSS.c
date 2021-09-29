// Audio Stream - Open Sound System
#define ENABLE_OSS_THREAD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>	// for open() constants
#include <unistd.h>	// for I/O calls
#include <sys/soundcard.h>	// OSS sound stuff
#include <sys/ioctl.h>

#ifdef ENABLE_OSS_THREAD
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include "../stdtype.h"

#include "AudioStream.h"
#include "../utils/OSThread.h"
#include "../utils/OSSignal.h"
#include "../utils/OSMutex.h"


typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
} WAVEFORMAT;	// from MSDN Help

#define WAVE_FORMAT_PCM	0x0001


typedef struct _oss_parameters
{
	int fragment;	// SNDCTL_DSP_SETFRAGMENT
	int format;		// SNDCTL_DSP_SETFMT
	int channels;	// SNDCTL_DSP_CHANNELS
	int smplrate;	// SNDCTL_DSP_SPEED
} OSS_PARAMS;
typedef struct _oss_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	WAVEFORMAT waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufSizeBits;
	UINT32 bufCount;
	UINT8* bufSpace;
	
#ifdef ENABLE_OSS_THREAD
	OS_THREAD* hThread;
#endif
	OS_SIGNAL* hSignal;
	OS_MUTEX* hMutex;
	int hFileDSP;
	volatile UINT8 pauseThread;
	
	void* userParam;
	AUDFUNC_FILLBUF FillBuffer;
	OSS_PARAMS ossParams;
} DRV_OSS;


UINT8 OSS_IsAvailable(void);
UINT8 OSS_Init(void);
UINT8 OSS_Deinit(void);
const AUDIO_DEV_LIST* OSS_GetDeviceList(void);
AUDIO_OPTS* OSS_GetDefaultOpts(void);

UINT8 OSS_Create(void** retDrvObj);
UINT8 OSS_Destroy(void* drvObj);
static UINT32 GetNearestBitVal(UINT32 value);
UINT8 OSS_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 OSS_Stop(void* drvObj);
UINT8 OSS_Pause(void* drvObj);
UINT8 OSS_Resume(void* drvObj);

UINT8 OSS_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 OSS_GetBufferSize(void* drvObj);
UINT8 OSS_IsBusy(void* drvObj);
UINT8 OSS_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 OSS_GetLatency(void* drvObj);
static void OssThread(void* Arg);


AUDIO_DRV audDrv_OSS =
{
	{ADRVTYPE_OUT, ADRVSIG_OSS, "OSS"},
	
	OSS_IsAvailable,
	OSS_Init, OSS_Deinit,
	OSS_GetDeviceList, OSS_GetDefaultOpts,
	
	OSS_Create, OSS_Destroy,
	OSS_Start, OSS_Stop,
	OSS_Pause, OSS_Resume,
	
	OSS_SetCallback, OSS_GetBufferSize,
	OSS_IsBusy, OSS_WriteData,
	
	OSS_GetLatency,
};


static char* ossDevNames[1] = {"/dev/dsp"};
static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 OSS_IsAvailable(void)
{
	int hFile;
	
	hFile = open(ossDevNames[0], O_WRONLY);
	if (hFile < 0)
		return 0;
	close(hFile);
	
	return 1;
}

UINT8 OSS_Init(void)
{
	if (isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = ossDevNames;
	
	
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

UINT8 OSS_Deinit(void)
{
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* OSS_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* OSS_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 OSS_Create(void** retDrvObj)
{
	DRV_OSS* drv;
	UINT8 retVal8;
	
	drv = (DRV_OSS*)malloc(sizeof(DRV_OSS));
	drv->devState = 0;
	drv->hFileDSP = 0;
#ifdef ENABLE_OSS_THREAD
	drv->hThread = NULL;
#endif
	drv->hSignal = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	retVal8  = OSSignal_Init(&drv->hSignal, 0);
	retVal8 |= OSMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		OSS_Destroy(drv);
		*retDrvObj = NULL;
		return AERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 OSS_Destroy(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (drv->devState != 0)
		OSS_Stop(drvObj);
#ifdef ENABLE_OSS_THREAD
	if (drv->hThread != NULL)
	{
		OSThread_Cancel(drv->hThread);
		OSThread_Deinit(drv->hThread);
	}
#endif
	if (drv->hSignal != NULL)
		OSSignal_Deinit(drv->hSignal);
	if (drv->hMutex != NULL)
		OSMutex_Deinit(drv->hMutex);
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

static UINT32 GetNearestBitVal(UINT32 value)
{
	// Example:
	//	16 -> nearest 16 (1<<4)
	//	23 -> nearest 16 (1<<4)
	//	24 -> nearest 32 (1<<5)
	// So I'll check, if the "nearest?" value is >= original value * 1.5
	UINT8 curBit;
	UINT32 newVal;
	
	value += value / 2;	// value *= 1.5
	for (curBit = 4, newVal = (1 << curBit); curBit < 32; curBit ++, newVal <<= 1)
	{
		if (newVal > value)
			return curBit - 1;
	}
	return 0;
}

UINT8 OSS_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	UINT64 tempInt64;
	int retVal;
#ifdef ENABLE_OSS_THREAD
	UINT8 retVal8;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	//printf("Wanted buffer size: %u\n", drv->bufSize);
	// We have the estimated buffer size now.
	drv->bufSizeBits = GetNearestBitVal(drv->bufSize);
	//Since OSS can only use buffers of the size 2^x, we need to recalculate everything here.
	drv->bufSize = 1 << drv->bufSizeBits;
	//printf("Used buffer size: %u\n", drv->bufSize);
	drv->bufSmpls = drv->bufSize / drv->waveFmt.nBlockAlign;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	drv->ossParams.fragment = (drv->bufCount << 16) | (drv->bufSizeBits << 0);
	// Note: While in the OSS documentation, not all systems define the 24 and 32-bit formats.
	if (drv->waveFmt.wBitsPerSample == 8)
		drv->ossParams.format = AFMT_U8;
	else if (drv->waveFmt.wBitsPerSample == 16)
		drv->ossParams.format = AFMT_S16_NE;
#ifdef AFMT_S24_PACKED
	else if (drv->waveFmt.wBitsPerSample == 24)
		drv->ossParams.format = AFMT_S24_PACKED;
#endif
#ifdef AFMT_S32_NE
	else if (drv->waveFmt.wBitsPerSample == 32)
		drv->ossParams.format = AFMT_S32_NE;
#endif
	else
		return 0xCF;	// invalid sample format
	drv->ossParams.channels = drv->waveFmt.nChannels;
	drv->ossParams.smplrate = drv->waveFmt.nSamplesPerSec;
	
	drv->hFileDSP = open(ossDevNames[0], O_WRONLY);
	if (drv->hFileDSP < 0)
	{
		drv->hFileDSP = 0;
		return 0xC0;		// open() failed
	}
	
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SETFRAGMENT, &drv->ossParams.fragment);
	if (retVal)
		printf("Error setting Fragment Size!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SETFMT, &drv->ossParams.format);
	if (retVal)
		printf("Error setting Format!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_CHANNELS, &drv->ossParams.channels);
	if (retVal)
		printf("Error setting Channels!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SPEED, &drv->ossParams.smplrate);
	if (retVal)
		printf("Error setting Sample Rate!\n");
	
	OSSignal_Reset(drv->hSignal);
#ifdef ENABLE_OSS_THREAD
	retVal8 = OSThread_Init(&drv->hThread, &OssThread, drv);
	if (retVal8)
	{
		retVal = close(drv->hFileDSP);
		drv->hFileDSP = 0;
		return 0xC8;	// CreateThread failed
	}
#endif
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	OSSignal_Signal(drv->hSignal);
	
	return AERR_OK;
}

UINT8 OSS_Stop(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
#ifdef ENABLE_OSS_THREAD
	OSThread_Join(drv->hThread);
	OSThread_Deinit(drv->hThread);	drv->hThread = NULL;
#endif
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = close(drv->hFileDSP);
	if (retVal)
		return 0xC4;		// close failed -- but why ???
	drv->hFileDSP = 0;
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 OSS_Pause(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread |= 0x01;
	return AERR_OK;
}

UINT8 OSS_Resume(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread &= ~0x01;
	return AERR_OK;
}


UINT8 OSS_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
#ifdef ENABLE_OSS_THREAD
	drv->pauseThread |= 0x02;
	OSMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	drv->pauseThread &= ~0x02;
	OSMutex_Unlock(drv->hMutex);
	
	return AERR_OK;
#else
	return AERR_NO_SUPPORT;
#endif
}

UINT32 OSS_GetBufferSize(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	return drv->bufSize;
}

UINT8 OSS_IsBusy(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	//return AERR_BUSY;
	return AERR_OK;
}

UINT8 OSS_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	ssize_t wrtBytes;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;
	
	wrtBytes = write(drv->hFileDSP, data, dataSize);
	if (wrtBytes == -1)
		return 0xFF;
	return AERR_OK;
}


UINT32 OSS_GetLatency(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	int retVal;
	int bytesBehind;
	
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_GETODELAY, &bytesBehind);
	if (retVal)
		return 0;
	
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void OssThread(void* Arg)
{
	DRV_OSS* drv = (DRV_OSS*)Arg;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 bufBytes;
	ssize_t wrtBytes;
	
	OSSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		OSMutex_Lock(drv->hMutex);
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, drv->bufSpace);
			wrtBytes = write(drv->hFileDSP, drv->bufSpace, bufBytes);
			didBuffers ++;
		}
		OSMutex_Unlock(drv->hMutex);
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
	}
	
	return;
}
