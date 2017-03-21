// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria
/***************************************************************************

  sn76496.c
  by Nicola Salmoria
  with contributions by others

  Routines to emulate the:
  Texas Instruments SN76489, SN76489A, SN76494/SN76496
  ( Also known as, or at least compatible with, the TMS9919 and SN94624.)
  and the Sega 'PSG' used on the Master System, Game Gear, and Megadrive/Genesis
  This chip is known as the Programmable Sound Generator, or PSG, and is a 4
  channel sound generator, with three squarewave channels and a noise/arbitrary
  duty cycle channel.

  Noise emulation for all verified chips should be accurate:

  ** SN76489 uses a 15-bit shift register with taps on bits D and E, output on E,
  XOR function.
  It uses a 15-bit ring buffer for periodic noise/arbitrary duty cycle.
  Its output is inverted.
  ** SN94624 is the same as SN76489 but lacks the /8 divider on its clock input.
  ** SN76489A uses a 15-bit shift register with taps on bits D and E, output on F,
  XOR function.
  It uses a 15-bit ring buffer for periodic noise/arbitrary duty cycle.
  Its output is not inverted.
  ** SN76494 is the same as SN76489A but lacks the /8 divider on its clock input.
  ** SN76496 is identical in operation to the SN76489A, but the audio input on pin 9 is
  documented.
  All the TI-made PSG chips have an audio input line which is mixed with the 4 channels
  of output. (It is undocumented and may not function properly on the sn76489, 76489a
  and 76494; the sn76489a input is mentioned in datasheets for the tms5200)
  All the TI-made PSG chips act as if the frequency was set to 0x400 if 0 is
  written to the frequency register.
  ** Sega Master System III/MD/Genesis PSG uses a 16-bit shift register with taps
  on bits C and F, output on F
  It uses a 16-bit ring buffer for periodic noise/arbitrary duty cycle.
  (whether it uses an XOR or XNOR needs to be verified, assumed XOR)
  (whether output is inverted or not needs to be verified, assumed to be inverted)
  ** Sega Game Gear PSG is identical to the SMS3/MD/Genesis one except it has an
  extra register for mapping which channels go to which speaker.
  The register, connected to a z80 port, means:
  for bits 7  6  5  4  3  2  1  0
           L3 L2 L1 L0 R3 R2 R1 R0
  Noise is an XOR function, and audio output is negated before being output.
  All the Sega-made PSG chips act as if the frequency was set to 0 if 0 is written
  to the frequency register.
  ** NCR7496 (as used on the Tandy 1000) is similar to the SN76489 but with a
  different noise LFSR patttern: taps on bits A and E, output on E
  It uses a 15-bit ring buffer for periodic noise/arbitrary duty cycle.
  (all this chip's info needs to be verified)

  28/03/2005 : Sebastien Chevalier
  Update th SN76496Write func, according to SN76489 doc found on SMSPower.
   - On write with 0x80 set to 0, when LastRegister is other then TONE,
   the function is similar than update with 0x80 set to 1

  23/04/2007 : Lord Nightmare
  Major update, implement all three different noise generation algorithms and a
  set_variant call to discern among them.

  28/04/2009 : Lord Nightmare
  Add READY line readback; cleaned up struct a bit. Cleaned up comments.
  Add more TODOs. Fixed some unsaved savestate related stuff.

  04/11/2009 : Lord Nightmare
  Changed the way that the invert works (it now selects between XOR and XNOR
  for the taps), and added R->OldNoise to simulate the extra 0 that is always
  output before the noise LFSR contents are after an LFSR reset.
  This fixes SN76489/A to match chips. Added SN94624.

  14/11/2009 : Lord Nightmare
  Removed STEP mess, vastly simplifying the code. Made output bipolar rather
  than always above the 0 line, but disabled that code due to pending issues.

  16/11/2009 : Lord Nightmare
  Fix screeching in regulus: When summing together four equal channels, the
  size of the max amplitude per channel should be 1/4 of the max range, not
  1/3. Added NCR7496.

  18/11/2009 : Lord Nightmare
  Modify Init functions to support negating the audio output. The gamegear
  psg does this. Change gamegear and sega psgs to use XOR rather than XNOR
  based on testing. Got rid of R->OldNoise and fixed taps accordingly.
  Added stereo support for game gear.

  15/01/2010 : Lord Nightmare
  Fix an issue with SN76489 and SN76489A having the wrong periodic noise periods.
  Note that properly emulating the noise cycle bit timing accurately may require
  extensive rewriting.

  24/01/2010: Lord Nightmare
  Implement periodic noise as forcing one of the XNOR or XOR taps to 1 or 0 respectively.
  Thanks to PlgDavid for providing samples which helped immensely here.
  Added true clock divider emulation, so sn94624 and sn76494 run 8x faster than
  the others, as in real life.

  15/02/2010: Lord Nightmare & Michael Zapf (additional testing by PlgDavid)
  Fix noise period when set to mirror channel 3 and channel 3 period is set to 0 (tested on hardware for noise, wave needs tests) - MZ
  Fix phase of noise on sn94624 and sn76489; all chips use a standard XOR, the only inversion is the output itself - LN, Plgdavid
  Thanks to PlgDavid and Michael Zapf for providing samples which helped immensely here.

  23/02/2011: Lord Nightmare & Enik
  Made it so the Sega PSG chips have a frequency of 0 if 0 is written to the
  frequency register, while the others have 0x400 as before. Should fix a bug
  or two on sega games, particularly Vigilante on Sega Master System. Verified
  on SMS hardware.

  27/06/2012: Michael Zapf
  Converted to modern device, legacy devices were gradually removed afterwards.

  16/09/2015: Lord Nightmare
  Fix PSG chips to have volume reg inited on reset to 0x0 based on tests by
  ValleyBell. Made Sega PSG chips start up with register 0x3 selected (volume
  for channel 2) based on hardware tests by Nemesis.

  TODO: * Implement the TMS9919 - any difference to sn94624?
        * Implement the T6W28; has registers in a weird order, needs writes
          to be 'sanitized' first. Also is stereo, similar to game gear.
        * Test the NCR7496; Smspower says the whitenoise taps are A and E,
          but this needs verification on real hardware.
        * Factor out common code so that the SAA1099 can share some code.

***************************************************************************/

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset()

#include <stdtype.h>
#include "../snddef.h"
#include "../EmuHelper.h"
#include "sn76496.h"


#define MAX_OUTPUT 0x8000


typedef struct _sn76496_state sn76496_state;
struct _sn76496_state
{
	DEV_DATA _devData;
	
	UINT32 clock;
	UINT32 feedback_mask;   // mask for feedback
	UINT32 whitenoise_tap1; // mask for white noise tap 1 (higher one, usually bit 14)
	UINT32 whitenoise_tap2; // mask for white noise tap 2 (lower one, usually bit 13)
	UINT8 negate;           // output negate flag
	UINT8 stereo;           // whether we're dealing with stereo or not
	UINT32 clock_divider;   // clock divider
	UINT8 sega_style_psg;   // flag for if frequency zero acts as if it is one more than max (0x3ff+1) or if it acts like 0; AND if the initial register is pointing to 0x3 instead of 0x0 AND if the volume reg is preloaded with 0xF instead of 0x0
	
	INT32 vol_table[16];    // volume table (for 4-bit to db conversion)
	UINT16 Register[8];     // registers
	UINT8 last_register;    // last register written
	INT32 volume[4];        // db volume of voice 0-2 and noise
	UINT32 RNG;             // noise generator LFSR
	//INT32 current_clock;
	UINT8 stereo_mask;      // the stereo output mask
	INT32 period[4];        // Length of 1/2 of waveform
	INT32 count[4];         // Position within the waveform
	UINT8 output[4];        // 1-bit output of each channel, pre-volume
	INT32 cycles_to_ready;  // number of cycles until the READY line goes active
	
	UINT8 ready_state;
	
	INT32 FNumLimit;
	UINT32 MuteMsk[4];
	UINT8 NgpFlags;         // bit 7 - NGP Mode on/off, bit 0 - is 2nd NGP chip
	sn76496_state* NgpChip2;    // pointer to other chip instance of T6W28
};


UINT8 sn76496_ready_r(void *chip, UINT8 offset)
{
	sn76496_state *R = (sn76496_state*)chip;
	return R->ready_state ? 1 : 0;
}

void sn76496_stereo_w(void *chip, UINT8 offset, UINT8 data)
{
	sn76496_state *R = (sn76496_state*)chip;
	if (R->stereo) R->stereo_mask = data;
	else logerror("sn76496_base_device: Call to stereo write with mono chip!\n");
}

void sn76496_write_reg(void *chip, UINT8 offset, UINT8 data)
{
	sn76496_state *R = (sn76496_state*)chip;
	UINT8 n, r, c;

	// set number of cycles until READY is active; this is always one
	// 'sample', i.e. it equals the clock divider exactly
	R->cycles_to_ready = 1;

	if (data & 0x80)
	{
		r = (data & 0x70) >> 4;
		R->last_register = r;
		R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
	}
	else
	{
		r = R->last_register;
	}

	c = r >> 1;
	switch (r)
	{
		case 0: // tone 0: frequency
		case 2: // tone 1: frequency
		case 4: // tone 2: frequency
			if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x0f) | ((data & 0x3f) << 4);
			if ((R->Register[r] != 0) || (R->sega_style_psg != 0)) R->period[c] = R->Register[r];
			else R->period[c] = 0x400;

			if (r == 4)
			{
				// update noise shift frequency
				if ((R->Register[6] & 0x03) == 0x03) R->period[3] = R->period[2]<<1;
			}
			break;
		case 1: // tone 0: volume
		case 3: // tone 1: volume
		case 5: // tone 2: volume
		case 7: // noise: volume
			R->volume[c] = R->vol_table[data & 0x0f];
			if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
			
		//	// "Every volume write resets the waveform to High level.", TmEE, 2012-11-24 on SMSPower
		//	R->output[c] = 1;
		//	R->count[c] = R->period[c];
		//	disabled for now - sounds awful
			break;
		case 6: // noise: frequency, mode
			{
				//if ((data & 0x80) == 0) logerror("sn76496_base_device: write to reg 6 with bit 7 clear; data was %03x, new write is %02x! report this to LN!\n", R->Register[6], data);
				if ((data & 0x80) == 0) R->Register[r] = (R->Register[r] & 0x3f0) | (data & 0x0f);
				n = R->Register[6]&3;
				// N/512,N/1024,N/2048,Tone #3 output
				R->period[3] = (n == 3) ? (R->period[2]<<1) : (2 << (4+n));
				R->RNG = R->feedback_mask;
			}
			break;
	}
}

INLINE UINT8 in_noise_mode(const sn76496_state *R)
{
	return ((R->Register[6] & 4)!=0);
}

INLINE void countdown_cycles(sn76496_state *R)
{
	if (R->cycles_to_ready > 0)
	{
		R->cycles_to_ready--;
		//if (R->ready_state == 1) R->ready_handler(CLEAR_LINE);
		R->ready_state = 0;
	}
	else
	{
		//if (!R->ready_state == 0) R->ready_handler(ASSERT_LINE);
		R->ready_state = 1;
	}
}

void SN76496Update(void* param, UINT32 samples, DEV_SMPL** outputs)
{
	UINT32 i;
	UINT32 j;
	sn76496_state *R = (sn76496_state *)param;
	sn76496_state *R2;
	DEV_SMPL* lbuffer = outputs[0];
	DEV_SMPL* rbuffer = outputs[1];
	DEV_SMPL out = 0;
	DEV_SMPL out2 = 0;
	INT32 vol[4];
	INT32 ggst[2];

	R2 = R->NgpFlags ? R->NgpChip2 : NULL;
	if (R->NgpFlags)
	{
		// Speed Hack
		out = 0;
		for (i = 0; i < 3; i ++)
		{
			if (R->period[i] || R->volume[i])
			{
				out = 1;
				break;
			}
		}
		if (R->volume[3])
			out = 1;
		if (! out)
		{
			memset(lbuffer, 0x00, sizeof(DEV_SMPL) * samples);
			memset(rbuffer, 0x00, sizeof(DEV_SMPL) * samples);
			return;
		}
	}
	
	ggst[0] = 0x01;
	ggst[1] = 0x01;
	for (j = 0; j < samples; j++)
	{
		// disabled, because dividing the output sample rate is easier and faster
	//	// clock chip once
	//	if (R->current_clock > 0) // not ready for new divided clock
	//	{
	//		R->current_clock--;
	//	}
	//	else // ready for new divided clock, make a new sample
	//	{
	//		R->current_clock = R->clock_divider-1;
			// decrement Cycles to READY by one
			countdown_cycles(R);

			// handle channels 0,1,2
			for (i = 0; i < 3; i++)
			{
				R->count[i]--;
				if (R->count[i] <= 0)
				{
					R->output[i] ^= 1;
					R->count[i] = R->period[i];
				}
			}

			// handle channel 3
			R->count[3]--;
			if (R->count[3] <= 0)
			{
				// if noisemode is 1, both taps are enabled
				// if noisemode is 0, the lower tap, whitenoisetap2, is held at 0
				// The != was a bit-XOR (^) before
				if (((R->RNG & R->whitenoise_tap1)!=0) != (((R->RNG & R->whitenoise_tap2)!=0) && in_noise_mode(R)))
				{
					R->RNG >>= 1;
					R->RNG |= R->feedback_mask;
				}
				else
				{
					R->RNG >>= 1;
				}
				R->output[3] = R->RNG & 1;

				R->count[3] = R->period[3];
			}
		//}

#if 0
		if (R->stereo)
		{
			out = ((((R->stereo_mask & 0x10)!=0) && (R->output[0]!=0))? R->volume[0] : 0)
				+ ((((R->stereo_mask & 0x20)!=0) && (R->output[1]!=0))? R->volume[1] : 0)
				+ ((((R->stereo_mask & 0x40)!=0) && (R->output[2]!=0))? R->volume[2] : 0)
				+ ((((R->stereo_mask & 0x80)!=0) && (R->output[3]!=0))? R->volume[3] : 0);

			out2= ((((R->stereo_mask & 0x1)!=0) && (R->output[0]!=0))? R->volume[0] : 0)
				+ ((((R->stereo_mask & 0x2)!=0) && (R->output[1]!=0))? R->volume[1] : 0)
				+ ((((R->stereo_mask & 0x4)!=0) && (R->output[2]!=0))? R->volume[2] : 0)
				+ ((((R->stereo_mask & 0x8)!=0) && (R->output[3]!=0))? R->volume[3] : 0);
		}
		else
		{
			out= ((R->output[0]!=0)? R->volume[0]:0)
				+((R->output[1]!=0)? R->volume[1]:0)
				+((R->output[2]!=0)? R->volume[2]:0)
				+((R->output[3]!=0)? R->volume[3]:0);
			out2 = out;
		}
#endif

		// --- CUSTOM CODE START --
		out = out2 = 0;
		if (! R->NgpFlags)
		{
			for (i = 0; i < 4; i ++)
			{
				// --- Preparation Start ---
				// Bipolar output
				vol[i] = R->output[i] ? +1 : -1;
				
				// Disable high frequencies (> SampleRate / 2) for tone channels
				// Freq. 0/1 isn't disabled becaus it would also disable PCM
				if (i != 3)
				{
					if (R->period[i] <= R->FNumLimit && R->period[i] > 1)
						vol[i] = 0;
				}
				vol[i] &= R->MuteMsk[i];
				// --- Preparation End ---
				
				if (R->stereo)
				{
					ggst[0] = (R->stereo_mask & (0x10 << i)) ? 1 : 0;
					ggst[1] = (R->stereo_mask & (0x01 << i)) ? 1 : 0;
				}
				if (R->period[i] > 1 || i == 3)
				{
					out += vol[i] * R->volume[i] * ggst[0];
					out2 += vol[i] * R->volume[i] * ggst[1];
				}
				else if (R->MuteMsk[i])
				{
					// Make Bipolar Output with PCM possible
					out += R->volume[i] * ggst[0];
					out2 += R->volume[i] * ggst[1];
				}
			}
		}
		else
		{
			if (! (R->NgpFlags & 0x01))
			{
				// Tone Channel 1-3
				if (R->stereo)
				{
					ggst[0] = (R->stereo_mask & (0x10 << i)) ? 1 : 0;
					ggst[1] = (R->stereo_mask & (0x01 << i)) ? 1 : 0;
				}
				for (i = 0; i < 3; i ++)
				{
					// --- Preparation Start ---
					// Bipolar output
					vol[i] = R->output[i] ? +1 : -1;
					
					// Disable high frequencies (> SampleRate / 2) for tone channels
					// Freq. 0 isn't disabled becaus it would also disable PCM
					if (R->period[i] <= R->FNumLimit && R->period[i] > 1)
						vol[i] = 0;
					vol[i] &= R->MuteMsk[i];
					// --- Preparation End ---
					
					if (R->period[i])
					{
						out += vol[i] * R->volume[i] * ggst[0];
						out2 += vol[i] * R2->volume[i] * ggst[1];
					}
					else if (R->MuteMsk[i])
					{
						// Make Bipolar Output with PCM possible
						out += R->volume[i] * ggst[0];
						out2 += R2->volume[i] * ggst[1];
					}
				}
			}
			else
			{
				// --- Preparation Start ---
				// Bipolar output
				vol[i] = R->output[i] ? +1 : -1;
				
				vol[i] &= R2->MuteMsk[i];	// use MuteMask from chip 0
				// --- Preparation End ---
				
				// Noise Channel
				if (R->stereo)
				{
					ggst[0] = (R->stereo_mask & 0x80) ? 1 : 0;
					ggst[1] = (R->stereo_mask & 0x08) ? 1 : 0;
				}
				else
				{
					ggst[0] = 1;
					ggst[1] = 1;
				}
				out += vol[3] * R2->volume[3] * ggst[0];
				out2 += vol[3] * R->volume[3] * ggst[1];
			}
		}
		// --- CUSTOM CODE END --
		
		if(R->negate) { out = -out; out2 = -out2; }

		lbuffer[j] = out >> 1;	// >>1 to make up for bipolar output
		rbuffer[j] = out2 >> 1;
	}
}



static void SN76496_set_gain(sn76496_state *R,int gain)
{
	int i;
	double out;


	gain &= 0xff;

	// increase max output basing on gain (0.2 dB per step)
	out = MAX_OUTPUT / 4; // four channels, each gets 1/4 of the total range
	while (gain-- > 0)
		out *= 1.023292992; // = (10 ^ (0.2/20))

	// build volume table (2dB per step)
	for (i = 0; i < 15; i++)
	{
		// limit volume to avoid clipping
		if (out > MAX_OUTPUT / 4) R->vol_table[i] = MAX_OUTPUT / 4;
		else R->vol_table[i] = (INT32)(out + 0.5);  // I like rounding

		out /= 1.258925412; /* = 10 ^ (2/20) = 2dB */
	}
	R->vol_table[15] = 0;
}


static UINT32 generic_start(sn76496_state *chip, UINT32 clock, UINT32 feedbackmask, UINT32 noisetap1, UINT32 noisetap2, UINT8 negate, UINT8 stereo, UINT8 clockdivider, UINT8 sega)
{
	UINT32 sample_rate;
	
	chip->clock = clock;
	chip->clock_divider = clockdivider ? clockdivider : 8;
	chip->feedback_mask = feedbackmask; // mask for feedback
	chip->whitenoise_tap1 = noisetap1;  // mask for white noise tap 1
	chip->whitenoise_tap2 = noisetap2;  // mask for white noise tap 2
	chip->negate = negate;              // channel negation
	chip->stereo = stereo;              // GameGear stereo
	chip->sega_style_psg = sega;        // frequency set to 0 results in freq = 0x400 rather than 0
	chip->NgpFlags = 0x00;
	chip->NgpChip2 = NULL;
	
	// set gain
	SN76496_set_gain(chip, 0);
	
	sn76496_set_mutemask(chip, 0x00);
	
	sample_rate = chip->clock / 2 / chip->clock_divider;
	
	return sample_rate;
}

UINT32 sn76496_start(void **chip, UINT32 clock, UINT8 shiftregwidth, UINT16 noisetaps,
					UINT8 negate, UINT8 stereo, UINT8 clockdivider, UINT8 sega)
{
	sn76496_state* sn_chip;
	UINT32 ntap[2];
	UINT8 curbit;
	UINT8 curtap;
	
	sn_chip = (sn76496_state*)calloc(1, sizeof(sn76496_state));
	if (sn_chip == NULL)
		return 0;
	*chip = sn_chip;
	
	// extract single noise tap bits
	ntap[0] = ntap[1] = 0x00;
	curtap = 0;
	for (curbit = 0; curbit < shiftregwidth; curbit ++)
	{
		if (noisetaps & (1 << curbit))
		{
			ntap[curtap] = (1 << curbit);
			curtap ++;
			if (curtap >= 2)
				break;
		}
	}
	
	return generic_start(sn_chip, clock, 1 << (shiftregwidth - 1), ntap[0], ntap[1],
						negate, stereo, clockdivider, sega);
}

void sn76496_connect_t6w28(void *noisechip, void *tonechip)
{
	sn76496_state *Rnoise = (sn76496_state *)noisechip;
	sn76496_state *Rtone = (sn76496_state *)tonechip;
	
	// Activate special NeoGeoPocket Mode
	Rtone->NgpFlags = 0x80 | 0x00;
	Rtone->NgpChip2 = Rnoise;
	Rnoise->NgpFlags = 0x80 | 0x01;
	Rnoise->NgpChip2 = Rtone;
	
	return;
}

void sn76496_shutdown(void *chip)
{
	sn76496_state *R = (sn76496_state*)chip;
	
	free(R);
	return;
}

void sn76496_reset(void *chip)
{
	sn76496_state *R = (sn76496_state*)chip;
	UINT8 i;
	
	for (i = 0; i < 4; i++) R->volume[i] = 0;

	R->last_register = R->sega_style_psg?3:0; // Sega VDP PSG defaults to selected period reg for 2nd channel
	for (i = 0; i < 8; i+=2)
	{
		R->Register[i] = 0;
		R->Register[i + 1] = 0x0;   // volume = 0x0 (max volume) on reset; this needs testing on chips other than SN76489A and Sega VDP PSG
	}

	for (i = 0; i < 4; i++)
	{
		R->output[i] = 0;
		R->period[i] = 0;
		R->count[i] = 0;
	}

	R->RNG = R->feedback_mask;
	R->output[3] = R->RNG & 1;

	R->cycles_to_ready = 1;          // assume ready is not active immediately on init. is this correct?
	R->stereo_mask = 0xFF;           // all channels enabled
	//R->current_clock = R->clock_divider-1;

	R->ready_state = 1;

	return;
}

void sn76496_freq_limiter(void* chip, UINT32 sample_rate)
{
	sn76496_state *R = (sn76496_state*)chip;
	
	R->FNumLimit = (R->clock / (2 * R->clock_divider)) / sample_rate;
	
	return;
}

void sn76496_set_mutemask(void *chip, UINT32 MuteMask)
{
	sn76496_state *R = (sn76496_state*)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		R->MuteMsk[CurChn] = (MuteMask & (1 << CurChn)) ? 0 : ~0;
	
	return;
}

// ---- MAME SN-settings ----
/*
// SN76496: Whitenoise verified, phase verified, periodic verified (by Michael Zapf)
sn76496_device::sn76496_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SN76496, "SN76496", tag, 0x10000, 0x04, 0x08, false, false, 8, true, owner, clock, "sn76496", __FILE__)
	{ }

// U8106 not verified yet. todo: verify; (a custom marked sn76489? only used on mr. do and maybe other universal games)
u8106_device::u8106_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, U8106, "U8106", tag, 0x4000, 0x01, 0x02, true, false, 8, true, owner, clock, "u8106", __FILE__)
	{ }

// Y2404 not verified yet. todo: verify; (don't be fooled by the Y, it's a TI chip, not Yamaha)
y2404_device::y2404_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, Y2404, "Y2404", tag, 0x10000, 0x04, 0x08, false, false, 8, true, owner, clock, "y2404", __FILE__)
	{ }

// SN76489 not verified yet. todo: verify;
sn76489_device::sn76489_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SN76489, "SN76489", tag, 0x4000, 0x01, 0x02, true, false, 8, true, owner, clock, "sn76489", __FILE__)
	{ }

// SN76489A: whitenoise verified, phase verified, periodic verified (by plgdavid)
sn76489a_device::sn76489a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SN76489A, "SN76489A", tag, 0x10000, 0x04, 0x08, false, false, 8, true, owner, clock, "sn76489a", __FILE__)
	{ }

// SN76494 not verified, (according to datasheet: same as sn76489a but without the /8 divider)
sn76494_device::sn76494_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SN76494, "SN76494", tag, 0x10000, 0x04, 0x08, false, false, 1, true, owner, clock, "sn76494", __FILE__)
	{ }

// SN94624 whitenoise verified, phase verified, period verified; verified by PlgDavid
sn94624_device::sn94624_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SN94624, "SN94624", tag, 0x4000, 0x01, 0x02, true, false, 1, true, owner, clock, "sn94624", __FILE__)
	{ }

// NCR7496 not verified; info from smspower wiki
ncr7496_device::ncr7496_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, NCR7496, "NCR7496", tag, 0x8000, 0x02, 0x20, false, false, 8, true, owner, clock, "ncr7496", __FILE__)
	{ }

// Verified by Justin Kerk
gamegear_device::gamegear_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, GAMEGEAR, "Game Gear PSG", tag, 0x8000, 0x01, 0x08, true, true, 8, false, owner, clock, "gamegear_psg", __FILE__)
	{ }

// todo: verify; from smspower wiki, assumed to have same invert as gamegear
segapsg_device::segapsg_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	:  sn76496_base_device(mconfig, SEGAPSG, "SEGA VDP PSG", tag, 0x8000, 0x01, 0x08, true, false, 8, false, owner, clock, "segapsg", __FILE__)
	{ }
*/
