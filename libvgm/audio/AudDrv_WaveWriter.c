// Audio Stream - Wave Writer
#define _CRTDBG_MAP_ALLOC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// for memcpy() etc.

#include "../common_def.h"	// stdtype.h, INLINE

#include "AudioStream.h"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

#pragma pack(1)
typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	//UINT16 cbSize;	// not required for WAVE_FORMAT_PCM
} WAVEFORMAT;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001


typedef struct _wave_writer_driver
{
	void* audDrvPtr;
	UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	char* fileName;
	FILE* hFile;
	WAVEFORMAT waveFmt;
	
	UINT32 hdrSize;
	UINT32 wrtDataBytes;
} DRV_WAV_WRT;


UINT8 WavWrt_IsAvailable(void);
UINT8 WavWrt_Init(void);
UINT8 WavWrt_Deinit(void);
const AUDIO_DEV_LIST* WavWrt_GetDeviceList(void);
AUDIO_OPTS* WavWrt_GetDefaultOpts(void);

UINT8 WavWrt_Create(void** retDrvObj);
UINT8 WavWrt_Destroy(void* drvObj);
UINT8 WavWrt_SetFileName(void* drvObj, const char* fileName);
const char* WavWrt_GetFileName(void* drvObj);

UINT8 WavWrt_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam);
UINT8 WavWrt_Stop(void* drvObj);
UINT8 WavWrt_PauseResume(void* drvObj);

UINT8 WavWrt_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 WavWrt_GetBufferSize(void* drvObj);
UINT8 WavWrt_IsBusy(void* drvObj);
UINT8 WavWrt_WriteData(void* drvObj, UINT32 dataSize, void* data);
UINT32 WavWrt_GetLatency(void* drvObj);

INLINE size_t fputLE16(UINT16 Value, FILE* hFile);
INLINE size_t fputLE32(UINT32 Value, FILE* hFile);
INLINE size_t fputBE32(UINT32 Value, FILE* hFile);


AUDIO_DRV audDrv_WaveWrt =
{
	{ADRVTYPE_DISK, ADRVSIG_WAVEWRT, "WaveWrite"},
	
	WavWrt_IsAvailable,
	WavWrt_Init, WavWrt_Deinit,
	WavWrt_GetDeviceList, WavWrt_GetDefaultOpts,
	
	WavWrt_Create, WavWrt_Destroy,
	WavWrt_Start, WavWrt_Stop,
	WavWrt_PauseResume, WavWrt_PauseResume,
	
	WavWrt_SetCallback, WavWrt_GetBufferSize,
	WavWrt_IsBusy, WavWrt_WriteData,
	
	WavWrt_GetLatency,
};


static char* wavOutDevNames[1] = {"Wave Writer"};
static AUDIO_OPTS defOptions;
static AUDIO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 WavWrt_IsAvailable(void)
{
	return 1;
}

UINT8 WavWrt_Init(void)
{
	if (isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = wavOutDevNames;
	
	
	memset(&defOptions, 0x00, sizeof(AUDIO_OPTS));
	defOptions.sampleRate = 44100;
	defOptions.numChannels = 2;
	defOptions.numBitsPerSmpl = 16;
	defOptions.usecPerBuf = 0;	// the Wave Writer has no buffer
	defOptions.numBuffers = 0;
	
	
	activeDrivers = 0;
	isInit = 1;
	
	return AERR_OK;
}

UINT8 WavWrt_Deinit(void)
{
	if (! isInit)
		return AERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return AERR_OK;
}

const AUDIO_DEV_LIST* WavWrt_GetDeviceList(void)
{
	return &deviceList;
}

AUDIO_OPTS* WavWrt_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 WavWrt_Create(void** retDrvObj)
{
	DRV_WAV_WRT* drv;
	
	drv = (DRV_WAV_WRT*)malloc(sizeof(DRV_WAV_WRT));
	drv->devState = 0;
	drv->fileName = NULL;
	drv->hFile = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return AERR_OK;
}

UINT8 WavWrt_Destroy(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	if (drv->devState != 0)
		WavWrt_Stop(drvObj);
	
	if (drv->fileName != NULL)
		free(drv->fileName);
	
	free(drv);
	activeDrivers --;
	
	return AERR_OK;
}

UINT8 WavWrt_SetFileName(void* drvObj, const char* fileName)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	if (drv->fileName != NULL)
		free(drv->fileName);
	drv->fileName = strdup(fileName);
	
	return AERR_OK;
}

const char* WavWrt_GetFileName(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	return drv->fileName;
}

UINT8 WavWrt_Start(void* drvObj, UINT32 deviceID, AUDIO_OPTS* options, void* audDrvParam)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	UINT32 DataLen;
	
	if (drv->devState != 0)
		return AERR_WASDONE;	// already running
	if (drv->fileName == NULL)
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
	
	
	drv->hFile = fopen(drv->fileName, "wb");
	if (drv->hFile == NULL)
		return AERR_FILE_ERR;
	
	drv->hdrSize = 0x00;
	fputBE32(0x52494646, drv->hFile);	// 'RIFF'
	fputLE32(0x00000000, drv->hFile);	// RIFF chunk length (dummy)
	drv->hdrSize += 0x08;
	
	fputBE32(0x57415645, drv->hFile);	// 'WAVE'
	fputBE32(0x666D7420, drv->hFile);	// 'fmt '
	DataLen = sizeof(WAVEFORMAT);
	fputLE32(DataLen, drv->hFile);		// format chunk legth
	
#ifndef VGM_BIG_ENDIAN
	fwrite(&drv->waveFmt, DataLen, 1, drv->hFile);
#else
	fputLE16(drv->waveFmt.wFormatTag,		drv->hFile);	// 0x00
	fputLE16(drv->waveFmt.nChannels,		drv->hFile);	// 0x02
	fputLE32(drv->waveFmt.nSamplesPerSec,	drv->hFile);	// 0x04
	fputLE32(drv->waveFmt.nAvgBytesPerSec,	drv->hFile);	// 0x08
	fputLE16(drv->waveFmt.nBlockAlign,		drv->hFile);	// 0x0C
	fputLE16(drv->waveFmt.wBitsPerSample,	drv->hFile);	// 0x0E
	//fputLE16(drv->waveFmt.cbSize,			drv->hFile);	// 0x10
#endif
	drv->hdrSize += 0x0C + DataLen;
	
	fputBE32(0x64617461, drv->hFile);	// 'data'
	fputLE32(0x00000000, drv->hFile);	// data chunk length (dummy)
	drv->hdrSize += 0x08;
	
	drv->wrtDataBytes = 0x00;
	drv->devState = 1;
	
	return AERR_OK;
}

UINT8 WavWrt_Stop(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	UINT32 riffLen;
	
	if (drv->devState != 1)
		return AERR_NOT_OPEN;
	
	drv->devState = 2;
	
	fseek(drv->hFile, drv->hdrSize - 0x04, SEEK_SET);
	fputLE32(drv->wrtDataBytes, drv->hFile);	// data chunk length
	
	fseek(drv->hFile, 0x0004, SEEK_SET);
	riffLen = drv->wrtDataBytes + drv->hdrSize - 0x08;
	fputLE32(riffLen, drv->hFile);				// RIFF chunk length
	
	fclose(drv->hFile);	drv->hFile = NULL;
	drv->devState = 0;
	
	return AERR_OK;
}

UINT8 WavWrt_PauseResume(void* drvObj)
{
	return AERR_NO_SUPPORT;
}


UINT8 WavWrt_SetCallback(void* drvObj, AUDFUNC_FILLBUF FillBufCallback, void* userParam)
{
	return AERR_NO_SUPPORT;
}

UINT32 WavWrt_GetBufferSize(void* drvObj)
{
	return 0;
}

UINT8 WavWrt_IsBusy(void* drvObj)
{
	return AERR_OK;
}

UINT8 WavWrt_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	UINT32 wrtBytes;
	
	if (drv->hFile == NULL)
		return AERR_NOT_OPEN;
	
	wrtBytes = (UINT32)fwrite(data, 0x01, dataSize, drv->hFile);
	if (! wrtBytes)
		return AERR_FILE_ERR;
	drv->wrtDataBytes += wrtBytes;
	
	return AERR_OK;
}

UINT32 WavWrt_GetLatency(void* drvObj)
{
	return 0;
}


INLINE size_t fputLE16(UINT16 Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x02, 1, hFile);
#else
	UINT8 dataArr[0x02];
	
	dataArr[0x00] = (Value & 0x00FF) >>  0;
	dataArr[0x01] = (Value & 0xFF00) >>  8;
	return fwrite(dataArr, 0x02, 1, hFile);
#endif
}

INLINE size_t fputLE32(UINT32 Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x04, 1, hFile);
#else
	UINT8 dataArr[0x04];
	
	dataArr[0x00] = (Value & 0x000000FF) >>  0;
	dataArr[0x01] = (Value & 0x0000FF00) >>  8;
	dataArr[0x02] = (Value & 0x00FF0000) >> 16;
	dataArr[0x03] = (Value & 0xFF000000) >> 24;
	return fwrite(dataArr, 0x04, 1, hFile);
#endif
}

INLINE size_t fputBE32(UINT32 Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	UINT8 dataArr[0x04];
	
	dataArr[0x00] = (Value & 0xFF000000) >> 24;
	dataArr[0x01] = (Value & 0x00FF0000) >> 16;
	dataArr[0x02] = (Value & 0x0000FF00) >>  8;
	dataArr[0x03] = (Value & 0x000000FF) >>  0;
	return fwrite(dataArr, 0x04, 1, hFile);
#else
	return fwrite(&Value, 0x04, 1, hFile);
#endif
}
