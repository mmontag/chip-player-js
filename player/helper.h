#ifndef __PLAYER_HELPER_H__
#define __PLAYER_HELPER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include <emu/EmuStructs.h>
#include <emu/Resampler.h>
#include <iconv.h>	// for iconv_t

typedef struct _vgm_base_device VGM_BASEDEV;
struct _vgm_base_device
{
	DEV_INFO defInf;
	RESMPL_STATE resmpl;
	VGM_BASEDEV* linkDev;
};

// callback function typedef for SetupLinkedDevices
typedef void (*SETUPLINKDEV_CB)(void* userParam, VGM_BASEDEV* cDev, DEVLINK_INFO* dLink);


void SetupLinkedDevices(VGM_BASEDEV* cBaseDev, SETUPLINKDEV_CB devCfgCB, void* cbUserParam);
void FreeDeviceTree(VGM_BASEDEV* cBaseDev, UINT8 freeBase);
UINT8 StrCharsetConv(iconv_t hIConv, size_t* outSize, char** outStr, size_t inSize, const char* inStr);

#ifdef __cplusplus
}
#endif

#endif	// __PLAYER_HELPER_H__
