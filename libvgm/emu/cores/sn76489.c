/* 
	SN76489 emulation
	by Maxim in 2001 and 2002
	converted from my original Delphi implementation

	I'm a C newbie so I'm sure there are loads of stupid things
	in here which I'll come back to some day and redo

	Includes:
	- Super-high quality tone channel "oversampling" by calculating fractional positions on transitions
	- Noise output pattern reverse engineered from actual SMS output
	- Volume levels taken from actual SMS output

	07/08/04  Charles MacDonald
	Modified for use with SMS Plus:
	- Added support for multiple PSG chips.
	- Added reset/config/update routines.
	- Added context management routines.
	- Removed SN76489_GetValues().
	- Removed some unused variables.
*/

#include <stdlib.h>	// malloc/free
#include <float.h>	// for FLT_MIN
#include <string.h>	// for memcpy

#include "../../stdtype.h"
#include "../snddef.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"
#include "sn76489.h"
#include "sn76489_private.h"
#include "../panning.h"


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, sn76496_w_maxim},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, sn76489_mute_maxim},
	{RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, sn76489_pan_maxim},
	{0x00, 0x00, 0, NULL}
};
DEV_DEF devDef_SN76489_Maxim =
{
	"SN76489", "Maxim", FCC_MAXM,
	
	(DEVFUNC_START)device_start_sn76496_maxim,
	(DEVFUNC_CTRL)SN76489_Shutdown,
	(DEVFUNC_CTRL)SN76489_Reset,
	(DEVFUNC_UPDATE)SN76489_Update,
	
	NULL,	// SetOptionBits
	(DEVFUNC_OPTMASK)sn76489_mute_maxim,
	(DEVFUNC_PANALL)sn76489_pan_maxim,
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};


#define NoiseInitialState 0x8000  /* Initial state of shift register */
#define PSG_CUTOFF        0x6     /* Value below which PSG does not output */

static const int PSGVolumeValues[16] = {
	/* These values are taken from a real SMS2's output */
//	{892,892,892,760,623,497,404,323,257,198,159,123,96,75,60,0}, /* I can't remember why 892... :P some scaling I did at some point */
	/* these values are true volumes for 2dB drops at each step (multiply previous by 10^-0.1) */
	/*1516,1205,957,760,603,479,381,303,240,191,152,120,96,76,60,0*/
// The MAME core uses 0x2000 as maximum volume (0x1000 for bipolar output)
	4096, 3254, 2584, 2053, 1631, 1295, 1029, 817, 649, 516, 410, 325, 258, 205, 163, 0
};


static SN76489_Context* SN76489_Init( UINT32 PSGClockValue, UINT32 SamplingRate)
{
	int i;
	SN76489_Context* chip = (SN76489_Context*)calloc(1, sizeof(SN76489_Context));
	if(chip)
	{
		chip->dClock=(float)PSGClockValue/16.0f/SamplingRate;
		
		SN76489_SetMute(chip, MUTE_ALLON);
		SN76489_Config(chip, FB_SEGAVDP, SRW_SEGAVDP);
		
		for( i = 0; i < 4; i++ )
			Panning_Centre(chip->panning[i]);
		//SN76489_Reset(chip);
		
		chip->NgpFlags = 0x00;
		chip->NgpChip2 = NULL;
	}
	return chip;
}

static void SN76489_ConnectT6W28(SN76489_Context* noisechip, SN76489_Context* tonechip)
{
	// Activate special NeoGeoPocket Mode
	tonechip->NgpFlags = 0x80 | 0x00;
	tonechip->NgpChip2 = noisechip;
	noisechip->NgpFlags = 0x80 | 0x01;
	noisechip->NgpChip2 = tonechip;
	
	return;
}

static void SN76489_Reset(SN76489_Context* chip)
{
	int i;

	chip->PSGStereo = 0xFF;

	for( i = 0; i <= 3; i++ )
	{
		/* Initialise PSG state */
		chip->Registers[2*i] = 1;		 /* tone freq=1 */
		chip->Registers[2*i+1] = 0xf;	 /* vol=off */
		chip->NoiseFreq = 0x10;

		/* Set counters to 0 */
		chip->ToneFreqVals[i] = 0;

		/* Set flip-flops to 1 */
		chip->ToneFreqPos[i] = 1;

		/* Set intermediate positions to do-not-use value */
		chip->IntermediatePos[i] = FLT_MIN;

		/* Set panning to centre */
		//Panning_Centre( chip->panning[i] );
	}

	chip->LatchedRegister = 0;

	/* Initialise noise generator */
	chip->NoiseShiftRegister = NoiseInitialState;

	/* Zero clock */
	chip->Clock = 0;
}

static void SN76489_Shutdown(SN76489_Context* chip)
{
	free(chip);
}

static void SN76489_Config(SN76489_Context* chip, UINT32 feedback, UINT8 sr_width)
{
	chip->WhiteNoiseFeedback = feedback;
	chip->SRWidth = sr_width;
}

static void SN76489_Write(SN76489_Context* chip, UINT8 data)
{
	if ( data & 0x80 )
	{
		/* Latch/data byte  %1 cc t dddd */
		chip->LatchedRegister = ( data >> 4 ) & 0x07;
		chip->Registers[chip->LatchedRegister] =
			( chip->Registers[chip->LatchedRegister] & 0x3f0 ) /* zero low 4 bits */
			| ( data & 0xf );                            /* and replace with data */
	} else {
		/* Data byte        %0 - dddddd */
		if ( !( chip->LatchedRegister % 2 ) && ( chip->LatchedRegister < 5 ) )
			/* Tone register */
			chip->Registers[chip->LatchedRegister] =
				( chip->Registers[chip->LatchedRegister] & 0x00f) /* zero high 6 bits */
				| ( ( data & 0x3f ) << 4 );                 /* and replace with data */
		else
			/* Other register */
			chip->Registers[chip->LatchedRegister]=data&0x0f; /* Replace with data */
	}
	switch (chip->LatchedRegister) {
	case 0:
	case 2:
	case 4: /* Tone channels */
		if ( chip->Registers[chip->LatchedRegister] == 0 )
			chip->Registers[chip->LatchedRegister] = 1; /* Zero frequency changed to 1 to avoid div/0 */
		break;
	case 6: /* Noise */
		chip->NoiseShiftRegister = NoiseInitialState;        /* reset shift register */
		chip->NoiseFreq = 0x10 << ( chip->Registers[6] & 0x3 ); /* set noise signal generator frequency */
		break;
	}
}

static void SN76489_GGStereoWrite(SN76489_Context* chip, UINT8 data)
{
	chip->PSGStereo=data;
}

static void SN76489_Update(SN76489_Context* chip, UINT32 length, DEV_SMPL **buffer)
{
	UINT32 i, j;
	SN76489_Context* chip_t;
	SN76489_Context* chip_n;
	
	if (! chip->NgpFlags)
	{
		chip_t = chip_n = chip;
	}
	else
	{
		if (! (chip->NgpFlags & 0x01))
		{
			chip_t = chip;
			chip_n = chip->NgpChip2;
		}
		else
		{
			chip_t = chip->NgpChip2;
			chip_n = chip;
		}
	}
	
	for( j = 0; j < length; j++ )
	{
		/* Tone channels */
		for ( i = 0; i <= 2; ++i )
			if ( (chip_t->Mute >> i) & 1 )
			{
				if ( chip_t->IntermediatePos[i] != FLT_MIN )
					/* Intermediate position (antialiasing) */
					chip->ChannelState[i] = chip_t->IntermediatePos[i];
				else
					/* Flat (no antialiasing needed) */
					chip->ChannelState[i] = (float)chip_t->ToneFreqPos[i];
			}
			else
				/* Muted channel */
				chip->ChannelState[i] = 0.0f;

		/* Noise channel */
		if ( (chip_t->Mute >> 3) & 1 )
		{
			//chip->Channels[3] = PSGVolumeValues[chip->Registers[7]] * ( chip_n->NoiseShiftRegister & 0x1 ) * 2; /* double noise volume */
			// Now the noise is bipolar, too. -Valley Bell
			chip->ChannelState[3] = (float)( (int)( chip_n->NoiseShiftRegister & 0x1 ) * 2 - 1 );
			// due to the way the white noise works here, it seems twice as loud as it should be
			if (chip_n->Registers[6] & 0x4 )
				chip->ChannelState[3] /= 2.0f;
		}
		else
			chip->ChannelState[3] = 0.0f;

		// Build stereo result into buffer
		buffer[0][j] = 0;
		buffer[1][j] = 0;
		if (! chip->NgpFlags)
		{
			// For all 4 channels
			for ( i = 0; i <= 3; ++i )
			{
				int chnOut = (int)(PSGVolumeValues[chip->Registers[2 * i + 1]] * chip->ChannelState[i]);
				if ( ( ( chip->PSGStereo >> i ) & 0x11 ) == 0x11 )
				{
					// no GG stereo for this channel
					buffer[0][j] += APPLY_PANNING_S( chnOut, chip->panning[i][0] ); // left
					buffer[1][j] += APPLY_PANNING_S( chnOut, chip->panning[i][1] ); // right
				}
				else
				{
					// GG stereo overrides panning
					buffer[0][j] += ( chip->PSGStereo >> (i+4) & 0x1 ) * chnOut; // left
					buffer[1][j] += ( chip->PSGStereo >>  i    & 0x1 ) * chnOut; // right
				}
			}
		}
		else
		{
			int chnOut;
			if (! (chip->NgpFlags & 0x01))
			{
				// For all 3 tone channels
				for (i = 0; i < 3; i ++)
				{
					chnOut = (int)(PSGVolumeValues[chip_t->Registers[2 * i + 1]] * chip->ChannelState[i]);
					buffer[0][j] += (chip->PSGStereo >> (i+4) & 0x1 ) * chnOut; // left
					chnOut = (int)(PSGVolumeValues[chip_n->Registers[2 * i + 1]] * chip->ChannelState[i]);
					buffer[1][j] += (chip->PSGStereo >>  i    & 0x1 ) * chnOut; // right
				}
			}
			else
			{
				// noise channel
				i = 3;
				chnOut = (int)(PSGVolumeValues[chip_t->Registers[2 * i + 1]] * chip->ChannelState[i]);
				buffer[0][j] += (chip->PSGStereo >> (i+4) & 0x1 ) * chnOut; // left
				chnOut = (int)(PSGVolumeValues[chip_n->Registers[2 * i + 1]] * chip->ChannelState[i]);
				buffer[1][j] += (chip->PSGStereo >>  i    & 0x1 ) * chnOut; // right
			}
		}

		/* Increment clock by 1 sample length */
		chip->Clock += chip->dClock;
		chip->NumClocksForSample = (UINT32)chip->Clock;   /* truncate */
		chip->Clock -= chip->NumClocksForSample;      /* remove integer part */
	
		/* Decrement tone channel counters */
		for ( i = 0; i <= 2; ++i )
			chip->ToneFreqVals[i] -= chip->NumClocksForSample;
	 
		/* Noise channel: match to tone2 or decrement its counter */
		if ( chip->NoiseFreq == 0x80 )
			chip->ToneFreqVals[3] = chip->ToneFreqVals[2];
		else
			chip->ToneFreqVals[3] -= chip->NumClocksForSample;
	
		/* Tone channels: */
		for ( i = 0; i <= 2; ++i ) {
			if ( chip->ToneFreqVals[i] <= 0 ) {   /* If the counter gets below 0... */
				if (chip->Registers[i*2]>=PSG_CUTOFF) {
					/* For tone-generating values, calculate how much of the sample is + and how much is - */
					/* This is optimised into an even more confusing state than it was in the first place... */
					chip->IntermediatePos[i] = ( chip->NumClocksForSample - chip->Clock + 2 * chip->ToneFreqVals[i] ) * chip->ToneFreqPos[i] / ( chip->NumClocksForSample + chip->Clock );
					/* Flip the flip-flop */
					chip->ToneFreqPos[i] = -chip->ToneFreqPos[i];
				} else {
					/* stuck value */
					chip->ToneFreqPos[i] = 1;
					chip->IntermediatePos[i] = FLT_MIN;
				}
				chip->ToneFreqVals[i] += chip->Registers[i*2] * ( chip->NumClocksForSample / chip->Registers[i*2] + 1 );
			}
			else
				/* signal no antialiasing needed */
				chip->IntermediatePos[i] = FLT_MIN;
		}
	
		/* Noise channel */
		if ( chip->ToneFreqVals[3] <= 0 ) {
			/* If the counter gets below 0... */
			/* Flip the flip-flop */
			chip->ToneFreqPos[3] = -chip->ToneFreqPos[3];
			if (chip->NoiseFreq != 0x80)
				/* If not matching tone2, decrement counter */
				chip->ToneFreqVals[3] += chip->NoiseFreq * ( chip->NumClocksForSample / chip->NoiseFreq + 1 );
			if (chip->ToneFreqPos[3] == 1) {
				/* On the positive edge of the square wave (only once per cycle) */
				UINT32 Feedback;
				if ( chip->Registers[6] & 0x4 ) {
					/* White noise */
					/* Calculate parity of fed-back bits for feedback */
					switch (chip->WhiteNoiseFeedback) {
						/* Do some optimised calculations for common (known) feedback values */
					case 0x0003: /* SC-3000, BBC %00000011 */
					case 0x0009: /* SMS, GG, MD  %00001001 */
						/* If two bits fed back, I can do Feedback=(nsr & fb) && (nsr & fb ^ fb) */
						/* since that's (one or more bits set) && (not all bits set) */
						Feedback = ( ( chip->NoiseShiftRegister & chip->WhiteNoiseFeedback )
							&& ( (chip->NoiseShiftRegister & chip->WhiteNoiseFeedback ) ^ chip->WhiteNoiseFeedback ) );
						break;
					default:
						/* Default handler for all other feedback values */
						/* XOR fold bits into the final bit */
						Feedback = chip->NoiseShiftRegister & chip->WhiteNoiseFeedback;
						Feedback ^= Feedback >> 8;
						Feedback ^= Feedback >> 4;
						Feedback ^= Feedback >> 2;
						Feedback ^= Feedback >> 1;
						Feedback &= 1;
						break;
					}
				} else	  /* Periodic noise */
					Feedback=chip->NoiseShiftRegister&1;

				chip->NoiseShiftRegister=(chip->NoiseShiftRegister>>1) | (Feedback << (chip->SRWidth-1));
			}
		}
	}
}


static UINT32 SN76489_GetMute(SN76489_Context* chip)
{
	return chip->Mute;
}

static void SN76489_SetMute(SN76489_Context* chip, UINT32 val)
{
	chip->Mute=(UINT8)val;
}

static void SN76489_SetPanning(SN76489_Context* chip, INT16 ch0, INT16 ch1, INT16 ch2, INT16 ch3)
{
	Panning_Calculate( chip->panning[0], ch0 );
	Panning_Calculate( chip->panning[1], ch1 );
	Panning_Calculate( chip->panning[2], ch2 );
	Panning_Calculate( chip->panning[3], ch3 );
}

static UINT8 device_start_sn76496_maxim(const SN76496_CFG* cfg, DEV_INFO* retDevInf)
{
	SN76489_Context* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->_genCfg.smplRate;
	chip = SN76489_Init(cfg->_genCfg.clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	chip->cfg = *cfg;
	if (cfg->t6w28_tone != NULL)
		SN76489_ConnectT6W28(chip, (SN76489_Context*)cfg->t6w28_tone);
	SN76489_Config(chip, cfg->noiseTaps, cfg->shiftRegWidth);
	
	devData = &chip->_devData;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_SN76489_Maxim);
	return 0x00;
}

static void sn76496_w_maxim(SN76489_Context* chip, UINT8 reg, UINT8 data)
{
	switch(reg)
	{
	case SN76496_W_REG:
		SN76489_Write(chip, data);
		break;
	case SN76496_W_GGST:
		SN76489_GGStereoWrite(chip, data);
		break;
	}
	return;
}

static void sn76489_mute_maxim(SN76489_Context* chip, UINT32 MuteMask)
{
	SN76489_SetMute(chip, ~MuteMask & 0x0F);
	return;
}

static void sn76489_pan_maxim(SN76489_Context* chip, const INT16* PanVals)
{
	SN76489_SetPanning(chip, PanVals[0x00], PanVals[0x01], PanVals[0x02], PanVals[0x03]);
	return;
}
