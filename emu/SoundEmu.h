#ifndef __SOUNDEMU_H__
#define __SOUNDEMU_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include "EmuStructs.h"

const DEV_DEF** SndEmu_GetDevDefList(UINT8 deviceID);	// returns &LIST_OF_(DEV_DEF)
UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
UINT8 SndEmu_Stop(DEV_INFO* devInf);
void SndEmu_FreeDevLinkData(DEV_INFO* devInf);
UINT8 SndEmu_GetDeviceFunc(const DEV_DEF* devInf, UINT8 funcType, UINT8 rwType, UINT16 reserved, void** retFuncPtr);


#define EERR_UNK_DEVICE	0xFF

#ifdef __cplusplus
}
#endif

#endif	// __SOUNDEMU_H__
