#ifndef __SOUNDEMU_H__
#define __SOUNDEMU_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include "EmuStructs.h"

UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
UINT8 SndEmu_Stop(DEV_INFO* devInf);


#define DEVID_SN76496	0x00

#define EERR_UNK_DEVICE	0xFF

#ifdef __cplusplus
}
#endif

#endif	// __SOUNDEMU_H__
