#ifndef __SOUNDEMU_H__
#define __SOUNDEMU_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include "EmuStructs.h"

typedef struct waveform_32bit_stereo
{
	DEV_SMPL L;
	DEV_SMPL R;
} WAVE_32BS;
typedef struct resampling_state
{
	UINT32 SmpRateSrc;
	UINT32 SmpRateDst;
	INT16 VolumeL;
	INT16 VolumeR;
	// Resampler Type:
	//	00 - Old
	//	01 - Upsampling
	//	02 - Copy
	//	03 - Downsampling
	UINT8 ResampleMode;	// can be FF [auto] or Resampler Type
	UINT8 Resampler;
	DEVFUNC_UPDATE StreamUpdate;
	void* SU_DataPtr;
	UINT32 SmpP;		// Current Sample (Playback Rate)
	UINT32 SmpLast;		// Sample Number Last
	UINT32 SmpNext;		// Sample Number Next
	WAVE_32BS LSmpl;	// Last Sample
	WAVE_32BS NSmpl;	// Next Sample
	UINT32 SmplBufSize;
	DEV_SMPL* SmplBufs[2];
} RESMPL_STATE;

UINT8 SndEmu_Start(UINT8 deviceID, const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
UINT8 SndEmu_Stop(DEV_INFO* devInf);
UINT8 SndEmu_GetDeviceFunc(const DEV_INFO* devInf, UINT8 funcType, UINT8 rwType, UINT16 reserved, void** retFuncPtr);

void SndEmu_ResamplerInit(RESMPL_STATE* CAA);
void SndEmu_ResamplerDeinit(RESMPL_STATE* CAA);
void SndEmu_ResamplerReset(void* DataPtr, UINT32 NewSmplRate);
void SndEmu_ResamplerExecute(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);


#define DEVID_SN76496	0x00

#define EERR_UNK_DEVICE	0xFF

#ifdef __cplusplus
}
#endif

#endif	// __SOUNDEMU_H__
