#include <stddef.h>

#include <stdtype.h>
#include "EmuStructs.h"
#include "SoundEmu.h"

#include "sn764intf.h"

UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	switch(deviceID)
	{
	case DEVID_SN76496:
		return device_start_sn76496((SN76496_CFG*)cfg, retDevInf);
	}
	return EERR_UNK_DEVICE;
}

UINT8 SndEmu_Stop(DEV_INFO* devInf)
{
	devInf->Stop(devInf->dataPtr);
	devInf->dataPtr = NULL;
	
	return 0x00;
}
