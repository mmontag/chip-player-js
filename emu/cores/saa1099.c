/***************************************************************************

    Philips SAA1099 Sound driver

    By Juergen Buchmueller and Manuel Abadia

    SAA1099 register layout:
    ========================

    offs | 7654 3210 | description
    -----+-----------+---------------------------
    0x00 | ---- xxxx | Amplitude channel 0 (left)
    0x00 | xxxx ---- | Amplitude channel 0 (right)
    0x01 | ---- xxxx | Amplitude channel 1 (left)
    0x01 | xxxx ---- | Amplitude channel 1 (right)
    0x02 | ---- xxxx | Amplitude channel 2 (left)
    0x02 | xxxx ---- | Amplitude channel 2 (right)
    0x03 | ---- xxxx | Amplitude channel 3 (left)
    0x03 | xxxx ---- | Amplitude channel 3 (right)
    0x04 | ---- xxxx | Amplitude channel 4 (left)
    0x04 | xxxx ---- | Amplitude channel 4 (right)
    0x05 | ---- xxxx | Amplitude channel 5 (left)
    0x05 | xxxx ---- | Amplitude channel 5 (right)
         |           |
    0x08 | xxxx xxxx | Frequency channel 0
    0x09 | xxxx xxxx | Frequency channel 1
    0x0a | xxxx xxxx | Frequency channel 2
    0x0b | xxxx xxxx | Frequency channel 3
    0x0c | xxxx xxxx | Frequency channel 4
    0x0d | xxxx xxxx | Frequency channel 5
         |           |
    0x10 | ---- -xxx | Channel 0 octave select
    0x10 | -xxx ---- | Channel 1 octave select
    0x11 | ---- -xxx | Channel 2 octave select
    0x11 | -xxx ---- | Channel 3 octave select
    0x12 | ---- -xxx | Channel 4 octave select
    0x12 | -xxx ---- | Channel 5 octave select
         |           |
    0x14 | ---- ---x | Channel 0 frequency enable (0 = off, 1 = on)
    0x14 | ---- --x- | Channel 1 frequency enable (0 = off, 1 = on)
    0x14 | ---- -x-- | Channel 2 frequency enable (0 = off, 1 = on)
    0x14 | ---- x--- | Channel 3 frequency enable (0 = off, 1 = on)
    0x14 | ---x ---- | Channel 4 frequency enable (0 = off, 1 = on)
    0x14 | --x- ---- | Channel 5 frequency enable (0 = off, 1 = on)
         |           |
    0x15 | ---- ---x | Channel 0 noise enable (0 = off, 1 = on)
    0x15 | ---- --x- | Channel 1 noise enable (0 = off, 1 = on)
    0x15 | ---- -x-- | Channel 2 noise enable (0 = off, 1 = on)
    0x15 | ---- x--- | Channel 3 noise enable (0 = off, 1 = on)
    0x15 | ---x ---- | Channel 4 noise enable (0 = off, 1 = on)
    0x15 | --x- ---- | Channel 5 noise enable (0 = off, 1 = on)
         |           |
    0x16 | ---- --xx | Noise generator parameters 0
    0x16 | --xx ---- | Noise generator parameters 1
         |           |
    0x18 | --xx xxxx | Envelope generator 0 parameters
    0x18 | x--- ---- | Envelope generator 0 control enable (0 = off, 1 = on)
    0x19 | --xx xxxx | Envelope generator 1 parameters
    0x19 | x--- ---- | Envelope generator 1 control enable (0 = off, 1 = on)
         |           |
    0x1c | ---- ---x | All channels enable (0 = off, 1 = on)
    0x1c | ---- --x- | Synch & Reset generators

***************************************************************************/

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "saa1099.h"


static void saa1099_write(void *info, UINT8 offset, UINT8 data);
static void saa1099_control_w(void *info, UINT8 data);
static void saa1099_data_w(void *info, UINT8 data);

static void saa1099_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_saa1099(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_saa1099(void *info);
static void device_reset_saa1099(void *info);

static void saa1099_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, saa1099_write},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"SAA1099", "MAME", FCC_MAME,
	
	device_start_saa1099,
	device_stop_saa1099,
	device_reset_saa1099,
	saa1099_update,
	
	NULL,	// SetOptionBits
	saa1099_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_SAA1099[] =
{
	&devDef,
	NULL
};


#define LEFT	0x00
#define RIGHT	0x01

/* this structure defines a channel */
struct saa1099_channel
{
	UINT8 frequency;			/* frequency (0x00..0xff) */
	UINT8 freq_enable;		/* frequency enable */
	UINT8 noise_enable;		/* noise enable */
	UINT8 octave; 			/* octave (0x00..0x07) */
	INT32 amplitude[2];		/* amplitude (0x00..0x0f) */
	UINT8 envelope[2];		/* envelope (0x00..0x0f or 0x10 == off) */

	/* vars to simulate the square wave */
	double counter;
	double freq;
	UINT8 level;
	UINT8 Muted;
};

/* this structure defines a noise channel */
struct saa1099_noise
{
	/* vars to simulate the noise generator output */
	double counter;
	double freq;
	UINT16 level;						/* noise polynomal shifter */
};

/* this structure defines a SAA1099 chip */
typedef struct _saa1099_state saa1099_state;
struct _saa1099_state
{
	void* chipInf;
	
	UINT8 noise_params[2];			/* noise generators parameters */
	UINT8 env_enable[2];			/* envelope generators enable */
	UINT8 env_reverse_right[2];		/* envelope reversed for right channel */
	UINT8 env_mode[2];				/* envelope generators mode */
	UINT8 env_bits[2];				/* non zero = 3 bits resolution */
	UINT8 env_clock[2];				/* envelope clock mode (non-zero external) */
	UINT8 env_step[2];				/* current envelope step */
	UINT8 all_ch_enable;			/* all channels enable */
	UINT8 sync_state;				/* sync all channels */
	UINT8 selected_reg;				/* selected register */
	struct saa1099_channel channels[6];	/* channels */
	struct saa1099_noise noise[2];	/* noise generators */
	double sample_rate;
	INT32 master_clock;
};

static const INT32 amplitude_lookup[16] = {
	 0*32768/16,  1*32768/16,  2*32768/16,  3*32768/16,
	 4*32768/16,  5*32768/16,  6*32768/16,  7*32768/16,
	 8*32768/16,  9*32768/16, 10*32768/16, 11*32768/16,
	12*32768/16, 13*32768/16, 14*32768/16, 15*32768/16
};

static const UINT8 envelope[8][64] = {
	/* zero amplitude */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* maximum amplitude */
	{15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	 15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15, },
	/* single decay */
	{15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive decay */
	{15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	/* single triangular */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive triangular */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	/* single attack */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* repetitive attack */
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
	  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 }
};


static void saa1099_envelope(saa1099_state *saa, int ch)
{
	if (saa->env_enable[ch])
	{
		UINT8 step, mode, mask;
		mode = saa->env_mode[ch];
		/* step from 0..63 and then loop in steps 32..63 */
		step = saa->env_step[ch] =
			((saa->env_step[ch] + 1) & 0x3f) | (saa->env_step[ch] & 0x20);

		mask = 15;
		if (saa->env_bits[ch])
			mask &= ~1; 	/* 3 bit resolution, mask LSB */

		saa->channels[ch*3+0].envelope[ LEFT] =
		saa->channels[ch*3+1].envelope[ LEFT] =
		saa->channels[ch*3+2].envelope[ LEFT] = envelope[mode][step] & mask;
		if (saa->env_reverse_right[ch] & 0x01)
		{
			saa->channels[ch*3+0].envelope[RIGHT] =
			saa->channels[ch*3+1].envelope[RIGHT] =
			saa->channels[ch*3+2].envelope[RIGHT] = (15 - envelope[mode][step]) & mask;
		}
		else
		{
			saa->channels[ch*3+0].envelope[RIGHT] =
			saa->channels[ch*3+1].envelope[RIGHT] =
			saa->channels[ch*3+2].envelope[RIGHT] = envelope[mode][step] & mask;
		}
	}
	else
	{
		/* envelope mode off, set all envelope factors to 16 */
		saa->channels[ch*3+0].envelope[ LEFT] =
		saa->channels[ch*3+1].envelope[ LEFT] =
		saa->channels[ch*3+2].envelope[ LEFT] =
		saa->channels[ch*3+0].envelope[RIGHT] =
		saa->channels[ch*3+1].envelope[RIGHT] =
		saa->channels[ch*3+2].envelope[RIGHT] = 16;
	}
}


static void saa1099_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	saa1099_state *saa = (saa1099_state *)param;
	UINT32 j, ch;
	INT32 clk2div512;

	/* if the channels are disabled we're done */
	if (!saa->all_ch_enable)
	{
		/* init output data */
		memset(outputs[LEFT],0,samples*sizeof(*outputs[LEFT]));
		memset(outputs[RIGHT],0,samples*sizeof(*outputs[RIGHT]));
		return;
	}

	for (ch = 0; ch < 2; ch++)
	{
		switch (saa->noise_params[ch])
		{
		case 0: saa->noise[ch].freq = saa->master_clock/ 256.0 * 2; break;
		case 1: saa->noise[ch].freq = saa->master_clock/ 512.0 * 2; break;
		case 2: saa->noise[ch].freq = saa->master_clock/1024.0 * 2; break;
		case 3: saa->noise[ch].freq = saa->channels[ch * 3].freq;   break;
		}
	}

	// clock fix thanks to http://www.vogons.org/viewtopic.php?p=344227#p344227
	//clk2div512 = 2 * saa->master_clock / 512;
	clk2div512 = (saa->master_clock + 128) / 256;
	
	/* fill all data needed */
	for( j = 0; j < samples; j++ )
	{
		DEV_SMPL output_l = 0, output_r = 0;

		/* for each channel */
		for (ch = 0; ch < 6; ch++)
		{
			struct saa1099_channel* saach = &saa->channels[ch];	// TODO: use

			if (saa->channels[ch].freq == 0.0)
				saa->channels[ch].freq = (double)(clk2div512 << saa->channels[ch].octave) /
					(511.0 - (double)saa->channels[ch].frequency);

			/* check the actual position in the square wave */
			saa->channels[ch].counter -= saa->channels[ch].freq;
			while (saa->channels[ch].counter < 0)
			{
				/* calculate new frequency now after the half wave is updated */
				saa->channels[ch].freq = (double)(clk2div512 << saa->channels[ch].octave) /
					(511.0 - (double)saa->channels[ch].frequency);

				saa->channels[ch].counter += saa->sample_rate;
				saa->channels[ch].level ^= 1;

				/* eventually clock the envelope counters */
				if (ch == 1 && saa->env_clock[0] == 0)
					saa1099_envelope(saa, 0);
				if (ch == 4 && saa->env_clock[1] == 0)
					saa1099_envelope(saa, 1);
			}

			if (saa->channels[ch].Muted)
				continue;	// placed here to ensure that envelopes are updated
			
#if 0
			// if the noise is enabled
			if (saa->channels[ch].noise_enable)
			{
				// if the noise level is high (noise 0: chan 0-2, noise 1: chan 3-5)
				if (saa->noise[ch/3].level & 1)
				{
					// subtract to avoid overflows, also use only half amplitude
					output_l -= saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 16 / 2;
					output_r -= saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 16 / 2;
				}
			}

			// if the square wave is enabled
			if (saa->channels[ch].freq_enable)
			{
				// if the channel level is high
				if (saa->channels[ch].level & 1)
				{
					output_l += saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 16;
					output_r += saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 16;
				}
			}
#else
			// Now with bipolar output. -Valley Bell
			if (saa->channels[ch].noise_enable)
			{
				// TODO: optimize (use variable for +1/-1)
				if (saa->noise[ch/3].level & 1)
				{
					output_l += saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 32 / 2;
					output_r += saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 32 / 2;
				}
				else
				{
					output_l -= saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 32 / 2;
					output_r -= saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 32 / 2;
				}
			}

			if (saa->channels[ch].freq_enable)
			{
				if (saa->channels[ch].level & 1)
				{
					output_l += saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 32;
					output_r += saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 32;
				}
				else
				{
					output_l -= saa->channels[ch].amplitude[ LEFT] * saa->channels[ch].envelope[ LEFT] / 32;
					output_r -= saa->channels[ch].amplitude[RIGHT] * saa->channels[ch].envelope[RIGHT] / 32;
				}
			}
#endif
		}

		for (ch = 0; ch < 2; ch++)
		{
			/* check the actual position in noise generator */
			saa->noise[ch].counter -= saa->noise[ch].freq;
			while (saa->noise[ch].counter < 0)
			{
				saa->noise[ch].counter += saa->sample_rate;
				if( ((saa->noise[ch].level & 0x4000) == 0) == ((saa->noise[ch].level & 0x0040) == 0) )
					saa->noise[ch].level = (saa->noise[ch].level << 1) | 1;
				else
					saa->noise[ch].level <<= 1;
			}
		}
		/* write sound data to the buffer */
		outputs[LEFT][j] = output_l / 6;
		outputs[RIGHT][j] = output_r / 6;
	}
}



static UINT8 device_start_saa1099(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	saa1099_state *saa;

	saa = (saa1099_state*)calloc(1, sizeof(saa1099_state));
	if (saa == NULL)
		return 0xFF;

	/* copy global parameters */
	saa->master_clock = cfg->clock;
	saa->sample_rate = saa->master_clock / 256.0;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, saa->sample_rate, cfg->smplRate);

	saa1099_set_mute_mask(saa, 0x00);
	
	saa->chipInf = saa;
	INIT_DEVINF(retDevInf, (DEV_DATA*)saa, (UINT32)(saa->sample_rate + 0.5), &devDef);
	
	return 0x00;
}

void device_stop_saa1099(void *info)
{
	saa1099_state *saa = (saa1099_state *)info;
	
	free(saa);
	
	return;
}

void device_reset_saa1099(void *info)
{
	saa1099_state *saa = (saa1099_state *)info;
	struct saa1099_channel *saach;
	UINT8 CurChn;

	for (CurChn = 0; CurChn < 6; CurChn ++)
	{
		saach = &saa->channels[CurChn];
		saach->frequency = 0;
		saach->octave = 0;
		saach->amplitude[0] = 0;
		saach->amplitude[1] = 0;
		saach->envelope[0] = 0;
		saach->envelope[1] = 0;
		saach->freq_enable = 0;
		saach->noise_enable = 0;
		
		saach->counter = 0;
		saach->freq = 0;
		saach->level = 0;
	}
	for (CurChn = 0; CurChn < 2; CurChn ++)
	{
		saa->noise[CurChn].counter = 0;
		saa->noise[CurChn].freq = 0;
		saa->noise[CurChn].level = 0;
		
		saa->noise_params[1] = 0x00;
		saa->env_reverse_right[CurChn] = 0x00;
		saa->env_mode[CurChn] = 0x00;
		saa->env_bits[CurChn] = 0x00;
		saa->env_clock[CurChn] = 0x00;
		saa->env_enable[CurChn] = 0x00;
		saa->env_step[CurChn] = 0;
	}
	
	saa->all_ch_enable = 0x00;
	saa->sync_state = 0x00;
	
	return;
}

static void saa1099_write(void *info, UINT8 offset, UINT8 data)
{
	if (offset & 1)
		saa1099_data_w(info, data);
	else
		saa1099_control_w(info, data);
}

static void saa1099_control_w(void *info, UINT8 data)
{
	saa1099_state *saa = (saa1099_state *)info;

	if ((data & 0xff) > 0x1c)
	{
		/* Error! */
		logerror("SAA1099: Unknown register selected\n");
	}

	saa->selected_reg = data & 0x1f;
	if (saa->selected_reg == 0x18 || saa->selected_reg == 0x19)
	{
		/* clock the envelope channels */
		if (saa->env_clock[0])
			saa1099_envelope(saa,0);
		if (saa->env_clock[1])
			saa1099_envelope(saa,1);
	}
}

static void saa1099_data_w(void *info, UINT8 data)
{
	saa1099_state *saa = (saa1099_state *)info;
	UINT8 reg = saa->selected_reg;
	int ch;

	switch (reg)
	{
	/* channel i amplitude */
	case 0x00:	case 0x01:	case 0x02:	case 0x03:	case 0x04:	case 0x05:
		ch = reg & 7;
		saa->channels[ch].amplitude[LEFT] = amplitude_lookup[data & 0x0f];
		saa->channels[ch].amplitude[RIGHT] = amplitude_lookup[(data >> 4) & 0x0f];
		break;
	/* channel i frequency */
	case 0x08:	case 0x09:	case 0x0a:	case 0x0b:	case 0x0c:	case 0x0d:
		ch = reg & 7;
		saa->channels[ch].frequency = data & 0xff;
		break;
	/* channel i octave */
	case 0x10:	case 0x11:	case 0x12:
		ch = (reg & 0x03) << 1;
		saa->channels[ch + 0].octave = data & 0x07;
		saa->channels[ch + 1].octave = (data >> 4) & 0x07;
		break;
	/* channel i frequency enable */
	case 0x14:
		saa->channels[0].freq_enable = data & 0x01;
		saa->channels[1].freq_enable = data & 0x02;
		saa->channels[2].freq_enable = data & 0x04;
		saa->channels[3].freq_enable = data & 0x08;
		saa->channels[4].freq_enable = data & 0x10;
		saa->channels[5].freq_enable = data & 0x20;
		break;
	/* channel i noise enable */
	case 0x15:
		saa->channels[0].noise_enable = data & 0x01;
		saa->channels[1].noise_enable = data & 0x02;
		saa->channels[2].noise_enable = data & 0x04;
		saa->channels[3].noise_enable = data & 0x08;
		saa->channels[4].noise_enable = data & 0x10;
		saa->channels[5].noise_enable = data & 0x20;
		break;
	/* noise generators parameters */
	case 0x16:
		saa->noise_params[0] = data & 0x03;
		saa->noise_params[1] = (data >> 4) & 0x03;
		break;
	/* envelope generators parameters */
	case 0x18:	case 0x19:
		ch = reg & 0x01;
		saa->env_reverse_right[ch] = data & 0x01;
		saa->env_mode[ch] = (data >> 1) & 0x07;
		saa->env_bits[ch] = data & 0x10;
		saa->env_clock[ch] = data & 0x20;
		saa->env_enable[ch] = data & 0x80;
		/* reset the envelope */
		saa->env_step[ch] = 0;
		break;
	/* channels enable & reset generators */
	case 0x1c:
		saa->all_ch_enable = data & 0x01;
		saa->sync_state = data & 0x02;
		if (data & 0x02)
		{
			int i;

			/* Synch & Reset generators */
			logerror("SAA1099: -reg 0x1c- Chip reset\n");
			for (i = 0; i < 6; i++)
			{
				saa->channels[i].level = 0;
				saa->channels[i].counter = 0.0;
			}
		}
		break;
	default:	/* Error! */
		logerror("SAA1099: Unknown operation (reg:%02x, data:%02x)\n",reg, data);
	}
}


static void saa1099_set_mute_mask(void *info, UINT32 MuteMask)
{
	saa1099_state *saa = (saa1099_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 6; CurChn ++)
		saa->channels[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
