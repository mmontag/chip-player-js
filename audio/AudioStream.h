#ifndef __AUDIOSTREAM_H__
#define __AUDIOSTREAM_H__

#ifdef __cplusplus
extern "C"
{
#endif

// Audio Drivers
#define AUDDRV_WINMM
#define AUDDRV_WAVEWRITE
#define AUDDRV_DSOUND
#define AUDDRV_XAUD2


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

UINT8 AudioDrv_SetCallback(void* drvStruct, AUDFUNC_FILLBUF FillBufCallback);
UINT8 AudioDrv_DataForward_Add(void* drvStruct, const void* destDrvStruct);
UINT8 AudioDrv_DataForward_Remove(void* drvStruct, const void* destDrvStruct);
UINT8 AudioDrv_DataForward_RemoveAll(void* drvStruct);

UINT8 AudioDrv_IsBusy(void* drvStruct);
UINT8 AudioDrv_WriteData(void* drvStruct, UINT32 dataSize, void* data);
UINT32 AudioDrv_GetLatency(void* drvStruct);

#ifdef __cplusplus
}
#endif

#endif	// __AUDIOSTREAM_H__
