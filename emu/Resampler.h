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
	UINT32 smpRateSrc;
	UINT32 smpRateDst;
	INT16 volumeL;
	INT16 volumeR;
	// Resampler Type:
	//	00 - Old
	//	01 - Upsampling
	//	02 - Copy
	//	03 - Downsampling
	UINT8 resampleMode;	// can be FF [auto] or Resampler Type
	UINT8 resampler;
	DEVFUNC_UPDATE StreamUpdate;
	void* su_DataPtr;
	UINT32 smpP;		// Current Sample (Playback Rate)
	UINT32 smpLast;		// Sample Number Last
	UINT32 smpNext;		// Sample Number Next
	WAVE_32BS lSmpl;	// Last Sample
	WAVE_32BS nSmpl;	// Next Sample
	UINT32 smplBufSize;
	DEV_SMPL* smplBufs[2];
} RESMPL_STATE;

// resampler helper functions (for quick/comfortable initialization)
void Resmpl_DevConnect(RESMPL_STATE* CAA, const DEV_INFO* devInf);
void Resmpl_SetVals(RESMPL_STATE* CAA, UINT8 resampleMode, UINT16 volume, UINT32 destSampleRate);
// resampler main functions
void Resmpl_Init(RESMPL_STATE* CAA);
void Resmpl_Deinit(RESMPL_STATE* CAA);
void Resmpl_ChangeRate(void* DataPtr, UINT32 newSmplRate);
void Resmpl_Execute(RESMPL_STATE* CAA, UINT32 samples, WAVE_32BS* smplBuffer);

#ifdef __cplusplus
}
#endif

#endif	// __RESAMPLER_H__
