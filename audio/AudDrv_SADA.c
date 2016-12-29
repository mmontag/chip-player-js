// Audio Stream - Solaris Audio Device Architecture
//	- libpthread (threads, optional)
#define ENABLE_SADA_THREAD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>	// for open() constants
#include <unistd.h>	// for I/O calls
#include <sys/audioio.h>

#ifdef ENABLE_SADA_THREAD
#include <pthread.h>	// for pthread functions
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include <stdtype.h>

#include "AudioStream.h"


#pragma pack(1)
typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
} WAVEFORMAT;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001


typedef struct _sada_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMAT waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
#ifdef ENABLE_SADA_THREAD
	pthread_t hThread;
#endif
	int hFileDSP;
	volatile UINT8 pauseThread;
	
	AUDFUNC_FILLBUF FillBuffer;
	struct audio_info sadaParams;
} DRV_SADA;


UINT8 SADA_IsAvailable(void);
UINT8 SADA_Init(void);
UINT8 SADA_Deinit(void);
const AUDIO_DEV_LIST* SADA_GetDeviceList(void);
AUDIO_OPTS* SADA_GetDefaultOpts(void);

UINT8 SADA_Create(void** retDrvObj);
UINT8 SADA_Destroy(void* drvObj);
UINT8 SADA_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 SADA_Stop(void* drvObj);
UINT8 SADA_Pause(void* drvObj);
UINT8 SADA_Resume(void* drvObj);

UINT8 SADA_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback);
UINT32 SADA_GetBufferSize(void* drvObj);
UINT8 SADA_IsBusy(void* drvObj);
UINT8 SADA_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 SADA_GetLatency(void* drvObj);
static void* SadaThread(void* Arg);


AUDIO_DRV audDrv_SADA =
{
	{ADRVTYPE_OUT, ADRVSIG_SADA, "SADA"},
	
	SADA_IsAvailable,
	SADA_Init, SADA_Deinit,
	SADA_GetDeviceList, SADA_GetDefaultOpts,
	
	SADA_Create, SADA_Destroy,
	SADA_Start, SADA_Stop,
	SADA_Pause, SADA_Resume,
	
	SADA_SetCallback, SADA_GetBufferSize,
	SADA_IsBusy, SADA_WriteData,
	
	SADA_GetLatency,
};


static char* ossDevNames[1] = {"/dev/audio"};
static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 SADA_IsAvailable(void)
{
	int hFile;
	
	hFile = open(ossDevNames[0], O_WRONLY);
	if (hFile < 0)
		return 0;
	close(hFile);
	
	return 1;
}

UINT8 SADA_Init(void)
{
	UINT32 numDevs;
	UINT32 curDev;
	UINT32 devLstID;
	
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

UINT8 SADA_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* SADA_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* SADA_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 SADA_Create(void** retDrvObj)
{
	DRV_SADA* drv;
	
	drv = (DRV_SADA*)malloc(sizeof(DRV_SADA));
	drv->devState = 0;
	drv->hFileDSP = 0;
#ifdef ENABLE_SADA_THREAD
	drv->hThread = 0;
#endif
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 SADA_Destroy(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (drv->devState != 0)
		SADA_Stop(drvObj);
#ifdef ENABLE_SADA_THREAD
	if (drv->hThread)
	{
		pthread_cancel(drv->hThread);
		pthread_join(drv->hThread, NULL);
		drv->hThread = 0;
	}
#endif
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 SADA_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	UINT64 tempInt64;
	int retVal;
	
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
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	AUDIO_INITINFO(&drv->sadaParams);
	drv->sadaParams.mode = AUMODE_PLAY;
	drv->sadaParams.play.sample_rate = drv->waveFmt.nSamplesPerSec;
	drv->sadaParams.play.channels = drv->waveFmt.nChannels;
	drv->sadaParams.play.precision = drv->waveFmt.wBitsPerSample;
	if (drv->waveFmt.wBitsPerSample == 8)
		drv->sadaParams.play.encoding = AUDIO_ENCODING_ULINEAR;
	else
		drv->sadaParams.play.encoding = AUDIO_ENCODING_SLINEAR;
	
	drv->hFileDSP = open(ossDevNames[0], O_WRONLY);
	if (drv->hFileDSP < 0)
	{
		drv->hFileDSP = 0;
		return 0xC0;		// open() failed
	}
	
	retVal = ioctl(drv->hFileDSP, AUDIO_SETINFO, &drv->sadaParams);
	if (retVal)
		printf("Error setting audio information!\n");
	
	drv->pauseThread = 1;
#ifdef ENABLE_SADA_THREAD
	drv->hThread = 0;
	retVal = pthread_create(&drv->hThread, NULL, &SadaThread, drv);
	if (retVal)
	{
		retVal = close(drv->hFileDSP);
		drv->hFileDSP = 0;
		return 0xC8;	// CreateThread failed
	}
#endif
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0;
	
	return AERR_OK;
}

UINT8 SADA_Stop(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
#ifdef ENABLE_SADA_THREAD
	if (drv->hThread)
	{
		pthread_join(drv->hThread, NULL);
		drv->hThread = 0;
	}
#endif
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = close(drv->hFileDSP);
	if (retVal)
		return 0xC4;		// close failed -- but why ???
	drv->hFileDSP = 0;
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 SADA_Pause(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread = 1;
	return AERR_OK;
}

UINT8 SADA_Resume(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread = 0;
	return AERR_OK;
}


UINT8 SADA_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
#ifdef ENABLE_SADA_THREAD
	drv->FillBuffer = FillBufCallback;
	
	return AERR_OK;
#else
	return AERR_NO_SUPPORT;
#endif
}

UINT32 SADA_GetBufferSize(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	return drv->bufSize;
}

UINT8 SADA_IsBusy(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	//return AERR_BUSY;
	return AERR_OK;
}

UINT8 SADA_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	ssize_t wrtBytes;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;
	
	wrtBytes = write(drv->hFileDSP, data, dataSize);
	if (wrtBytes == -1)
		return 0xFF;
	return AERR_OK;
}


UINT32 SADA_GetLatency(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	int retVal;
	int bytesBehind;
	
	// TODO: Find out what the Output-Delay call for SADA is.
//	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_GETODELAY, &bytesBehind);
//	if (retVal)
//		return 0;
	bytesBehind = 0;
	
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void* SadaThread(void* Arg)
{
	DRV_SADA* drv = (DRV_SADA*)Arg;
	UINT32 curBuf;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 bufBytes;
	ssize_t wrtBytes;
	
	while(drv->pauseThread)
		Sleep(1);
	while(drv->devState == 1)
	{
		didBuffers = 0;
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->bufSize, drv->bufSpace);
			wrtBytes = write(drv->hFileDSP, drv->bufSpace, bufBytes);
			didBuffers ++;
		}
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
	}
	
	return 0;
}
