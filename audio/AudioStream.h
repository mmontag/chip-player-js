#ifndef __AUDIOSTREAM_H__
#define __AUDIOSTREAM_H__

#ifdef __cplusplus
extern "C"
{
#endif

// Audio Drivers
/*
#define AUDDRV_WAVEWRITE

#ifdef _WIN32

#define AUDDRV_WINMM
#define AUDDRV_DSOUND
#define AUDDRV_XAUD2
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define AUDDRV_WASAPI	// no WASAPI for MS VC6 or MinGW
#endif

#else

#define AUDDRV_OSS
//#define AUDDRV_SADA
#define AUDDRV_ALSA
#define AUDDRV_LIBAO

#endif
*/

#include "AudioStructs.h"

UINT8 Audio_Init(void);
UINT8 Audio_Deinit(void);
UINT32 Audio_GetDriverCount(void);
UINT8 Audio_GetDriverInfo(UINT32 drvID, AUDDRV_INFO** retDrvInfo);

UINT8 AudioDrv_Init(UINT32 drvID, void** retDrvStruct);
UINT8 AudioDrv_Deinit(void** drvStruct);
const AUDIO_DEV_LIST* AudioDrv_GetDeviceList(void* drvStruct);
AUDIO_OPTS* AudioDrv_GetOptions(void* drvStruct);
void* AudioDrv_GetDrvData(void* drvStruct);

UINT8 AudioDrv_Start(void* drvStruct, UINT32 devID);
UINT8 AudioDrv_Stop(void* drvStruct);
UINT32 AudioDrv_Pause(void* drvStruct);
UINT32 AudioDrv_Resume(void* drvStruct);

// sets a callback function that will be called whenever a buffer is free
UINT8 AudioDrv_SetCallback(void* drvStruct, AUDFUNC_FILLBUF FillBufCallback);
// Using Data Forwarding, you can tell the audio system to send a copy of all
// data the audio driver receives to one or multiple other drivers.
// This can be used e.g. to log all data that is played.
UINT8 AudioDrv_DataForward_Add(void* drvStruct, const void* destDrvStruct);
UINT8 AudioDrv_DataForward_Remove(void* drvStruct, const void* destDrvStruct);
UINT8 AudioDrv_DataForward_RemoveAll(void* drvStruct);

// returns the maximum number of bytes that can be written using AudioDrv_WriteData()
// Note: only valid after calling AudioDrv_Start()
UINT32 AudioDrv_GetBufferSize(void* drvStruct);
UINT8 AudioDrv_IsBusy(void* drvStruct);
UINT8 AudioDrv_WriteData(void* drvStruct, UINT32 dataSize, void* data);
// returns current latency of the audio device in milliseconds
UINT32 AudioDrv_GetLatency(void* drvStruct);

#ifdef __cplusplus
}
#endif

#endif	// __AUDIOSTREAM_H__
