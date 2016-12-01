#ifndef __RESAMPLER_H__
#define __RESAMPLER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdtype.h>
#include "snddef.h"	// for DEV_SMPL
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

// resampler helper functions (for quick/comfortable initialization)
void Resmpl_DevConnect(RESMPL_STATE* CAA, const DEV_INFO* devInf);
void Resmpl_SetVals(RESMPL_STATE* CAA, UINT8 resampleMode, UINT16 volume, UINT32 destSampleRate);
// resampler main functions
void Resmpl_Init(RESMPL_STATE* CAA);
void Resmpl_Deinit(RESMPL_STATE* CAA);
void Resmpl_ChangeRate(void* DataPtr, UINT32 NewSmplRate);
void Resmpl_Execute(RESMPL_STATE* CAA, UINT32 Length, WAVE_32BS* RetSample);

#ifdef __cplusplus
}
#endif

#endif	// __RESAMPLER_H__
