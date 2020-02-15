#ifndef __SN76489_PRIVATE_H__
#define __SN76489_PRIVATE_H__

#include "../../stdtype.h"
#include "../snddef.h"
#include "sn764intf.h"


/*
    More testing is needed to find and confirm feedback patterns for
    SN76489 variants and compatible chips.
*/
enum feedback_patterns {
	FB_BBCMICRO =   0x8005, /* Texas Instruments TMS SN76489N (original) from BBC Micro computer */
	FB_SC3000   =   0x0006, /* Texas Instruments TMS SN76489AN (rev. A) from SC-3000H computer */
	FB_SEGAVDP  =   0x0009, /* SN76489 clone in Sega's VDP chips (315-5124, 315-5246, 315-5313, Game Gear) */
};

enum sr_widths {
	SRW_SC3000BBCMICRO  = 15,
	SRW_SEGAVDP = 16
};

enum volume_modes {
	VOL_TRUNC   =   0,      /* Volume levels 13-15 are identical */
	VOL_FULL    =   1,      /* Volume levels 13-15 are unique */
};

enum mute_values {
	MUTE_ALLOFF =   0,      /* All channels muted */
	MUTE_TONE1  =   1,      /* Tone 1 mute control */
	MUTE_TONE2  =   2,      /* Tone 2 mute control */
	MUTE_TONE3  =   4,      /* Tone 3 mute control */
	MUTE_NOISE  =   8,      /* Noise mute control */
	MUTE_ALLON  =   15,     /* All channels enabled */
};

typedef struct _SN76489_Context SN76489_Context;
struct _SN76489_Context
{
	DEV_DATA _devData;
	SN76496_CFG cfg;
	
	UINT8 Mute;				/* per-channel muting */
	
	/* Variables */
	float Clock;
	float dClock;
	UINT8 PSGStereo;
	UINT32 NumClocksForSample;
	UINT32 WhiteNoiseFeedback;
	UINT8 SRWidth;
	
	/* PSG registers: */
	UINT16 Registers[8];	/* Tone, vol x4 */
	UINT8 LatchedRegister;
	UINT32 NoiseShiftRegister;
	UINT16 NoiseFreq;		/* Noise channel signal generator frequency */
	
	/* Output calculation variables */
	INT32 ToneFreqVals[4];	/* Frequency register values (counters) */
	INT32 ToneFreqPos[4];	/* Frequency channel flip-flops */
	float IntermediatePos[4];	/* intermediate values used at boundaries between + and - (does not need double accuracy)*/
	float ChannelState[4];	/* output state of each channel, before volume/stereo is applied */
	
	INT32 panning[4][2];	/* fake stereo */
	
	UINT8 NgpFlags;			/* bit 7 - NGP Mode on/off, bit 0 - is 2nd NGP chip */
	SN76489_Context* NgpChip2;
};

/* Function prototypes */
static SN76489_Context* SN76489_Init(UINT32 PSGClockValue, UINT32 SamplingRate);
static void SN76489_ConnectT6W28(SN76489_Context* noisechip, SN76489_Context* tonechip);
static void SN76489_Reset(SN76489_Context* chip);
static void SN76489_Shutdown(SN76489_Context* chip);
static void SN76489_Config(SN76489_Context* chip, UINT32 feedback, UINT8 sr_width);
static void SN76489_Write(SN76489_Context* chip, UINT8 data);
static void SN76489_GGStereoWrite(SN76489_Context* chip, UINT8 data);
static void SN76489_Update(SN76489_Context* chip, UINT32 length, DEV_SMPL **buffer);

/* Non-standard getters and setters */
static UINT32 SN76489_GetMute(SN76489_Context* chip);
static void SN76489_SetMute(SN76489_Context* chip, UINT32 val);

static void SN76489_SetPanning(SN76489_Context* chip, INT16 ch0, INT16 ch1, INT16 ch2, INT16 ch3);

static UINT8 device_start_sn76496_maxim(const SN76496_CFG* cfg, DEV_INFO* retDevInf);
static void sn76496_w_maxim(SN76489_Context* chip, UINT8 reg, UINT8 data);
static void sn76489_mute_maxim(SN76489_Context* chip, UINT32 MuteMask);
static void sn76489_pan_maxim(SN76489_Context* chip, INT16* PanVals);

#endif	// __SN76489_PRIVATE_H__
