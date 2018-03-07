// Audio Stream - Advanced Linux Sound Architecture
//	- libasound
//	- libpthread (threads)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alsa/asoundlib.h>

#include <pthread.h>	// for pthread functions
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)

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


typedef struct _alsa_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMAT waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	pthread_t hThread;
	snd_pcm_t* hPCM;
	volatile UINT8 pauseThread;
	UINT8 canPause;
	
	void* userParam;
	AUDFUNC_FILLBUF FillBuffer;
} DRV_ALSA;


UINT8 ALSA_IsAvailable(void);
UINT8 ALSA_Init(void);
UINT8 ALSA_Deinit(void);
const AUDIO_DEV_LIST* ALSA_GetDeviceList(void);
AUDIO_OPTS* ALSA_GetDefaultOpts(void);

UINT8 ALSA_Create(void** retDrvObj);
UINT8 ALSA_Destroy(void* drvObj);
UINT8 ALSA_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 ALSA_Stop(void* drvObj);
UINT8 ALSA_Pause(void* drvObj);
UINT8 ALSA_Resume(void* drvObj);

UINT8 ALSA_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 ALSA_GetBufferSize(void* drvObj);
UINT8 ALSA_IsBusy(void* drvObj);
UINT8 ALSA_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 ALSA_GetLatency(void* drvObj);
static void* AlsaThread(void* Arg);
static UINT8 WriteBuffer(DRV_ALSA* drv, UINT32 dataSize, void* data);


AUDIO_DRV audDrv_ALSA =
{
	{ADRVTYPE_OUT, ADRVSIG_ALSA, "ALSA"},
	
	ALSA_IsAvailable,
	ALSA_Init, ALSA_Deinit,
	ALSA_GetDeviceList, ALSA_GetDefaultOpts,
	
	ALSA_Create, ALSA_Destroy,
	ALSA_Start, ALSA_Stop,
	ALSA_Pause, ALSA_Resume,
	
	ALSA_SetCallback, ALSA_GetBufferSize,
	ALSA_IsBusy, ALSA_WriteData,
	
	ALSA_GetLatency,
};


static char* alsaDevNames[1] = {"default"};
static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 ALSA_IsAvailable(void)
{
	return 1;
}

UINT8 ALSA_Init(void)
{
	UINT32 numDevs;
	UINT32 curDev;
	UINT32 devLstID;
	
	if (isInit)
		return AERR_WASDONE;
	
	// TODO: Make device list (like aplay -l)
	// This will need a LUT for deviceID -> hw:num1,num2.
	deviceList.devCount = 1;
	deviceList.devNames = alsaDevNames;
	
	
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

UINT8 ALSA_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* ALSA_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* ALSA_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 ALSA_Create(void** retDrvObj)
{
	DRV_ALSA* drv;
	
	drv = (DRV_ALSA*)malloc(sizeof(DRV_ALSA));
	drv->devState = 0;
	drv->hPCM = NULL;
	drv->hThread = 0;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 ALSA_Destroy(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	if (drv->devState != 0)
		ALSA_Stop(drvObj);
	if (drv->hThread)
	{
		pthread_cancel(drv->hThread);
		pthread_join(drv->hThread, NULL);
		drv->hThread = 0;
	}
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 ALSA_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	UINT64 tempInt64;
	int retVal;
	snd_pcm_hw_params_t* sndParams;
	snd_pcm_format_t sndPcmFmt;
	snd_pcm_uframes_t periodSize;
	snd_pcm_uframes_t bufferSize;
	snd_pcm_uframes_t oldBufSize;
	int rateDir = 0;
	
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
	//drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	if (drv->waveFmt.wBitsPerSample == 8)
		sndPcmFmt = SND_PCM_FORMAT_U8;
	else if (drv->waveFmt.wBitsPerSample == 16)
		sndPcmFmt = SND_PCM_FORMAT_S16;
	else if (drv->waveFmt.wBitsPerSample == 24)
		sndPcmFmt = SND_PCM_FORMAT_S24;
	else if (drv->waveFmt.wBitsPerSample == 32)
		sndPcmFmt = SND_PCM_FORMAT_S32;
	else
		return 0xCF;
	
	retVal = snd_pcm_open(&drv->hPCM, alsaDevNames[0], SND_PCM_STREAM_PLAYBACK, 0x00);
	if (retVal < 0)
		return 0xC0;		// snd_pcm_open() failed
	
	snd_pcm_hw_params_alloca(&sndParams);
	snd_pcm_hw_params_any(drv->hPCM, sndParams);
	
	snd_pcm_hw_params_set_access(drv->hPCM, sndParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(drv->hPCM, sndParams, sndPcmFmt);
	snd_pcm_hw_params_set_channels(drv->hPCM, sndParams, drv->waveFmt.nChannels);
	snd_pcm_hw_params_set_rate_near(drv->hPCM, sndParams, &drv->waveFmt.nSamplesPerSec, &rateDir);
	if (rateDir)
		drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	periodSize = drv->bufSmpls;
	bufferSize = drv->bufSmpls * drv->bufCount;
	oldBufSize = bufferSize;
	//printf("Wanted buffers: segment %u, count %u, total %u\n", drv->bufSmpls, drv->bufCount, oldBufSize);
	snd_pcm_hw_params_set_period_size_near(drv->hPCM, sndParams, &periodSize, &rateDir);
	if (rateDir)
	{
		drv->bufSmpls = periodSize;
		//printf("Fixed segment %u\n", drv->bufSmpls);
	}
	snd_pcm_hw_params_set_buffer_size_near(drv->hPCM, sndParams, &bufferSize);
	if (bufferSize != oldBufSize)
	{
		drv->bufCount = (bufferSize + periodSize / 2) / periodSize;
		//printf("fix count %u, total %u (%u)\n", drv->bufCount, bufferSize, drv->bufCount * periodSize);
	}
	
	drv->canPause = (UINT8)snd_pcm_hw_params_can_pause(sndParams);
	
	retVal = snd_pcm_hw_params(drv->hPCM, sndParams);
	// disabled for now, as it sometimes causes a double-free crash
	//snd_pcm_hw_params_free(sndParams);
	if (retVal < 0)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return 0xCF;
	}
	
	drv->pauseThread = 1;
	drv->hThread = 0;
	retVal = pthread_create(&drv->hThread, NULL, &AlsaThread, drv);
	if (retVal)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return 0xC8;	// CreateThread failed
	}
	
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0;
	
	return AERR_OK;
}

UINT8 ALSA_Stop(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	if (drv->hThread)
	{
		pthread_join(drv->hThread, NULL);
		drv->hThread = 0;
	}
	
	if (drv->canPause)
		retVal = snd_pcm_pause(drv->hPCM, 0);
	retVal = snd_pcm_drain(drv->hPCM);
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = snd_pcm_close(drv->hPCM);
	if (retVal < 0)
		return 0xC4;		// close failed -- but why ???
	drv->hPCM = NULL;
	
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 ALSA_Pause(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return 0xFF;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 1);
		if (retVal < 0)
			return AERR_API_ERR;
	}
	drv->pauseThread = 1;
	return AERR_OK;
}

UINT8 ALSA_Resume(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return 0xFF;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 0);
		if (retVal < 0)
			return AERR_API_ERR;
	}
	drv->pauseThread = 0;
	return AERR_OK;
}


UINT8 ALSA_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	
	return AERR_OK;
}

UINT32 ALSA_GetBufferSize(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	return drv->bufSize;
}

UINT8 ALSA_IsBusy(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	snd_pcm_sframes_t frmCount;
	
	if (drv->FillBuffer != NULL)
		return AERR_BAD_MODE;
	
	frmCount = snd_pcm_avail_update(drv->hPCM);
	return (frmCount > 0) ? AERR_OK : AERR_BUSY;
}

UINT8 ALSA_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (dataSize > drv->bufSize)
		return AERR_TOO_MUCH_DATA;
	
	return WriteBuffer(drv, dataSize, data);
}


UINT32 ALSA_GetLatency(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	snd_pcm_sframes_t smplsBehind;
	
	retVal = snd_pcm_delay(drv->hPCM, &smplsBehind);
	if (retVal < 0)
		return 0;
	return smplsBehind * 1000 / drv->waveFmt.nSamplesPerSec;
}

static void* AlsaThread(void* Arg)
{
	DRV_ALSA* drv = (DRV_ALSA*)Arg;
	UINT32 curBuf;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 bufBytes;
	int retVal;
	
	while(drv->pauseThread)
		Sleep(1);
	while(drv->devState == 1)
	{
		didBuffers = 0;
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			retVal = snd_pcm_wait(drv->hPCM, 5);
			// Note: On errors I try to send some data in order to call recovery functions.
			if (retVal != 0)
			{
				bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, drv->bufSpace);
				retVal = WriteBuffer(drv, bufBytes, drv->bufSpace);
			}
			didBuffers ++;	// not 100% correct, but has the desired effect
		}
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
	}
	
	return 0;
}

static UINT8 WriteBuffer(DRV_ALSA* drv, UINT32 dataSize, void* data)
{
	int retVal;
	
	do
	{
		retVal = snd_pcm_writei(drv->hPCM, data, dataSize / drv->waveFmt.nBlockAlign);
	} while (retVal == -EAGAIN);
	if (retVal == -ESTRPIPE)
	{
		retVal = snd_pcm_resume(drv->hPCM);
		while(retVal == -EAGAIN)
		{
			Sleep(1);
			retVal = snd_pcm_resume(drv->hPCM);
		}
		if (retVal < 0)
			retVal = -EPIPE;
	}
	if (retVal == -EPIPE)
	{
		// buffer underrun
		snd_pcm_prepare(drv->hPCM);
	}
	
	return AERR_OK;
}
