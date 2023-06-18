#ifndef __ASTRM_SPCDRVFUNS_H__
#define __ASTRM_SPCDRVFUNS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef AUDDRV_DSOUND
#include <windows.h>	// for HWND
#endif


#ifdef AUDDRV_WAVEWRITE
UINT8 WavWrt_SetFileName(void* drvObj, const char* fileName);
const char* WavWrt_GetFileName(void* drvObj);
#endif

#ifdef AUDDRV_DSOUND
UINT8 DSound_SetHWnd(void* drvObj, HWND hWnd);
#endif

#ifdef AUDDRV_PULSE
UINT8 Pulse_SetStreamDesc(void* drvObj, const char* fileName);
const char* Pulse_GetStreamDesc(void* drvObj);
#endif

#ifdef __cplusplus
}
#endif

#endif	// __ASTRM_SPCDRVFUNS_H__
