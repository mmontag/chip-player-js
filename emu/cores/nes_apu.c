// license:GPL-2.0+
// copyright-holders:Matthew Conte
/*****************************************************************************

  MAME/MESS NES APU CORE

  Based on the Nofrendo/Nosefart NES N2A03 sound emulation core written by
  Matthew Conte (matt@conte.com) and redesigned for use in MAME/MESS by
  Who Wants to Know? (wwtk@mail.com)

  This core is written with the advise and consent of Matthew Conte and is
  released under the GNU Public License.  This core is freely available for
  use in any freeware project, subject to the following terms:

  Any modifications to this code must be duly noted in the source and
  approved by Matthew Conte and myself prior to public submission.

  timing notes:
  master = 21477270
  2A03 clock = master/12
  sequencer = master/89490 or CPU/7457

 *****************************************************************************

   NES_APU.C

   Actual NES APU interface.

   LAST MODIFIED 02/29/2004

   - Based on Matthew Conte's Nofrendo/Nosefart core and redesigned to
     use MAME system calls and to enable multiple APUs.  Sound at this
     point should be just about 100% accurate, though I cannot tell for
     certain as yet.

     A queue interface is also available for additional speed.  However,
     the implementation is not yet 100% (DPCM sounds are inaccurate),
     so it is disabled by default.

 *****************************************************************************

   BUGFIXES:

   - Various bugs concerning the DPCM channel fixed. (Oliver Achten)
   - Fixed $4015 read behaviour. (Oliver Achten)

 *****************************************************************************/

#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../../stdbool.h"
#include "../../common_def.h"
#include "../snddef.h"
#include "../panning.h"
#include "nes_apu.h"

/* AN EXPLANATION
 *
 * The NES APU is actually integrated into the Nintendo processor.
 * You must supply the same number of APUs as you do processors.
 * Also make sure to correspond the memory regions to those used in the
 * processor, as each is shared.
 */

#include "nes_defs.h"

/* GLOBAL CONSTANTS */
#define  SYNCS_MAX1     0x20
#define  SYNCS_MAX2     0x80
#define  NTSC_APU_CLOCK (21477272 / 12)
#define  PAL_APU_CLOCK  (26601712 / 16)

/* GLOBAL VARIABLES */
struct _nesapu_state
{
	DEV_DATA _devData;

	apu_t   APU;                   /* Actual APUs */
	uint8   is_pal;
	uint8   nonlinear_mixing;
	float   apu_incsize;           /* Adjustment increment */
	uint32  samps_per_sync;        /* Number of samples per vsync */
	uint32  buffer_size;           /* Actual buffer size in bytes */
	uint32  real_rate;             /* Actual playback rate */
	uint32  vbl_times[0x20];       /* VBL durations in samples */
	uint32  sync_times1[SYNCS_MAX1]; /* Samples per sync table */
	uint32  sync_times2[SYNCS_MAX2]; /* Samples per sync table */
};

static DEV_SMPL square_lut[31];       // Non-linear Square wave output LUT
static DEV_SMPL tnd_lut[16][16][128]; // Non-linear Triangle, Noise, DMC output LUT
static UINT8 tablesInit = 0;

static UINT8 DPCMBase0 = 0x01;

/* INTERNAL FUNCTIONS */

/* INITIALIZE WAVE TIMES RELATIVE TO SAMPLE RATE */
static void create_vbltimes(uint32 * table,const uint8 *vbl,unsigned int rate)
{
	int i;

	for (i = 0; i < 0x20; i++)
		table[i] = vbl[i] * rate;
}

/* INITIALIZE SAMPLE TIMES IN TERMS OF VSYNCS */
static void create_syncs(nesapu_state *info, unsigned long sps)
{
	int i;
	unsigned long val = sps;

	for (i = 0; i < SYNCS_MAX1; i++)
		info->sync_times1[i] = sps * (i + 1);

	for (i = 0; i < SYNCS_MAX2; i++)
		info->sync_times2[i] = (sps * i) >> 2;
}

static void create_mixer_lut(void)
{
	int i, t;

	if (tablesInit)
		return;
	tablesInit = 0;

	// calculate mixer output
	/*
	pulse channel output:

	         95.88
	-----------------------
	      8128
	----------------- + 100
	pulse 1 + pulse 2

	*/
	for (i = 0; i < 31; i++)
	{
		double pulse_out = (i == 0) ? 0.0 : 95.88 / ((8128.0 / i) + 100.0);
		square_lut[i] = (DEV_SMPL)(pulse_out * 0x8000);
	}

	/*
	triangle, noise, DMC channel output:

	             159.79
	-------------------------------
	            1
	------------------------- + 100
	triangle   noise    dmc
	-------- + ----- + -----
	  8227     12241   22638

	*/
	for (t = 0; t < 16; t++)
	{
		int n;
		for (n = 0; n < 16; n++)
		{
			int d;
			for (d = 0; d < 128; d++)
			{
				double tnd_out = (t / 8227.0) + (n / 12241.0) + (d / 22638.0);
				tnd_out = (tnd_out == 0.0) ? 0.0 : 159.79 / ((1.0 / tnd_out) + 100.0);
				tnd_lut[t][n][d] = (DEV_SMPL)(tnd_out * 0x8000);
			}
		}
	}
}

/* TODO: sound channels should *ALL* have DC volume decay */

/* OUTPUT SQUARE WAVE SAMPLE (VALUES FROM -16 to +15) */
static void apu_square(nesapu_state *info, square_t *chan)
{
	int env_delay;
	int sweep_delay;
	uint8 freq_index;

	/* reg0: 0-3=volume, 4=envelope, 5=hold, 6-7=duty cycle
	** reg1: 0-2=sweep shifts, 3=sweep inc/dec, 4-6=sweep length, 7=sweep on
	** reg2: 8 bits of freq
	** reg3: 0-2=high freq, 7-4=vbl length counter
	*/

	if (!chan->enabled || chan->Muted)
	{
		chan->output = 0;
		return;
	}

	/* enveloping */
	env_delay = info->sync_times1[chan->regs[0] & 0x0f];

	/* decay is at a rate of (env_regs + 1) / 240 secs */
	chan->env_phase -= 4;
	while (chan->env_phase < 0)
	{
		chan->env_phase += env_delay;
		if (chan->regs[0] & 0x20)
			chan->env_vol = (chan->env_vol + 1) & 15;
		else if (chan->env_vol < 15)
			chan->env_vol++;
	}

	/* vbl length counter */
	if (chan->vbl_length > 0 && !(chan->regs [0] & 0x20))
		chan->vbl_length--;

	if (!chan->vbl_length)
	{
		chan->output = 0;
		return;
	}

	/* freqsweeps */
	if ((chan->regs[1] & 0x80) && (chan->regs[1] & 7))
	{
		sweep_delay = info->sync_times1[(chan->regs[1] >> 4) & 7];
		chan->sweep_phase -= 2;
		while (chan->sweep_phase < 0)
		{
			chan->sweep_phase += sweep_delay;
			if (chan->regs[1] & 8)
				chan->freq -= chan->freq >> (chan->regs[1] & 7);
			else
				chan->freq += chan->freq >> (chan->regs[1] & 7);
		}
	}

	// Thanks to Delek for the fix
	if (chan->regs[1] & 0x80)
		freq_index = chan->regs[1] & 7;	//If sweeping is enabled, I choose it as normal.
	else
		freq_index = 7;	//If sweeping is disabled, I choose the lower limit.
	//if ((!(chan->regs[1] & 8) && (chan->freq >> 16) > freq_limit[chan->regs[1] & 7])	// original
	if ((!(chan->regs[1] & 8) && (chan->freq >> 16) > freq_limit[freq_index])	// fixed
		 || (chan->freq >> 16) < 4)
	{
		chan->output = 0;
		return;
	}

	chan->phaseacc -= (float) info->apu_incsize; /* # of cycles per sample */

	while (chan->phaseacc < 0)
	{
		chan->phaseacc += (chan->freq >> 16);
		chan->adder = (chan->adder + 1) & 0x0f;
	}

	if (chan->regs[0] & 0x10) /* fixed volume */
		chan->output = chan->regs[0] & 0x0F;
	else
		chan->output = 0x0f - chan->env_vol;

	//BIT( duty_lut[chan->regs[0] >> 6],  7 - BIT(chan->adder, 1, 3) );
	if (duty_lut[chan->regs[0] >> 6] & (1 << (7 - (chan->adder >> 1))))
		chan->output = -chan->output;
}

/* OUTPUT TRIANGLE WAVE SAMPLE (VALUES FROM -16 to +15) */
static void apu_triangle(nesapu_state *info, triangle_t *chan)
{
	int freq;
	/* reg0: 7=holdnote, 6-0=linear length counter
	** reg2: low 8 bits of frequency
	** reg3: 7-3=length counter, 2-0=high 3 bits of frequency
	*/

	if (! chan->enabled || chan->Muted)
	{
		chan->output = 0;
		return;
	}

	if (!chan->counter_started && !(chan->regs[0] & 0x80))
	{
		if (chan->write_latency)
			chan->write_latency--;
		if (!chan->write_latency)
			chan->counter_started = true;
	}

	if (chan->counter_started)
	{
		if (chan->linear_length > 0)
			chan->linear_length--;
		if (chan->vbl_length && !(chan->regs[0] & 0x80))
			chan->vbl_length--;

		if (!chan->vbl_length)
		{
			chan->output = 0;
			return;
		}
	}

	if (!chan->linear_length)
	{
		chan->output = 0;
		return;
	}

	freq = (((chan->regs[3] & 7) << 8) + chan->regs[2]) + 1;

	if (freq < 4) /* inaudible */
	{
		chan->output = 0;
		return;
	}

	chan->phaseacc -= (float) info->apu_incsize; /* # of cycles per sample */
	while (chan->phaseacc < 0)
	{
		chan->phaseacc += freq;
		chan->adder = (chan->adder + 1) & 0x1f;

		chan->output = chan->adder & 0x0f;
		if (chan->adder & 8)
			chan->output ^= 0x07;
		else
			chan->output ^= 0x08;
		if (chan->adder & 0x10)
			chan->output ^= 0x0F;
		chan->output = (chan->output << 1) - 0x10;
	}
}

/* OUTPUT NOISE WAVE SAMPLE (VALUES FROM -16 to +15) */
static void apu_noise(nesapu_state *info, noise_t *chan)
{
	int freq, env_delay;
	uint8 outvol;

	/* reg0: 0-3=volume, 4=envelope, 5=hold
	** reg2: 7=small(93 byte) sample,3-0=freq lookup
	** reg3: 7-4=vbl length counter
	*/

	if (!chan->enabled || chan->Muted)
	{
		chan->output = 0;
		return;
	}

	/* enveloping */
	env_delay = info->sync_times1[chan->regs[0] & 0x0f];

	/* decay is at a rate of (env_regs + 1) / 240 secs */
	chan->env_phase -= 4;
	while (chan->env_phase < 0)
	{
		chan->env_phase += env_delay;
		if (chan->regs[0] & 0x20)
			chan->env_vol = (chan->env_vol + 1) & 15;
		else if (chan->env_vol < 15)
			chan->env_vol++;
	}

	/* length counter */
	if (!(chan->regs[0] & 0x20))
	{
		if (chan->vbl_length > 0)
			chan->vbl_length--;
	}

	if (!chan->vbl_length)
	{
		chan->output = 0;
		return;
	}

	freq = noise_freq[info->is_pal][chan->regs[2] & 0x0f];
	chan->phaseacc -= (float) info->apu_incsize; /* # of cycles per sample */
	while (chan->phaseacc < 0)
	{
		uint32 feedback;
		chan->phaseacc += freq;

		feedback = chan->seed ^ (chan->seed >> ((chan->regs[2] & 0x80) ? 6 : 1));
		chan->seed = (chan->seed >> 1) | ((feedback & 1) << 14);
	}

	if (chan->regs[0] & 0x10) /* fixed volume */
		outvol = chan->regs[0] & 0x0f;
	else
		outvol = 0x0F - chan->env_vol;

	chan->output = (chan->seed & 1) ? outvol : -outvol;
}

/* RESET DPCM PARAMETERS */
INLINE void apu_dpcmreset(dpcm_t *chan)
{
	chan->address = 0xc000 + (uint16) (chan->regs[2] << 6);
	chan->length = (uint16) (chan->regs[3] << 4) + 1;
	chan->bits_left = chan->length << 3;
	chan->irq_occurred = false;
	chan->enabled = true; /* Fixed * Proper DPCM channel ENABLE/DISABLE flag behaviour*/
	// Note: according to NSFPlay, it does NOT do that
	chan->vol = 0; /* Fixed * DPCM DAC resets itself when restarted */
}

/* OUTPUT DPCM WAVE SAMPLE (VALUES FROM -64 to +63) */
/* TODO: centerline naughtiness */
static void apu_dpcm(nesapu_state *info, dpcm_t *chan)
{
	int freq, bit_pos;

	/* reg0: 7=irq gen, 6=looping, 3-0=pointer to clock table
	** reg1: output dc level, 7 bits unsigned
	** reg2: 8 bits of 64-byte aligned address offset : $C000 + (value * 64)
	** reg3: length, (value * 16) + 1
	*/
	if (chan->Muted)
	{
		chan->output = 0;
		return;
	}

	if (chan->enabled)
	{
		freq = dpcm_clocks[info->is_pal][chan->regs[0] & 0x0f];
		chan->phaseacc -= (float) info->apu_incsize; /* # of cycles per sample */

		while (chan->phaseacc < 0)
		{
			chan->phaseacc += freq;

			if (!chan->length)
			{
				chan->enabled = false; /* Fixed * Proper DPCM channel ENABLE/DISABLE flag behaviour*/
				chan->vol = 0; /* Fixed * DPCM DAC resets itself when restarted */
				if (chan->regs[0] & 0x40)
					apu_dpcmreset(chan);
				else
				{
					if (chan->regs[0] & 0x80) /* IRQ Generator */
					{
						chan->irq_occurred = true;
						//n2a03_irq(info->APU.dpcm.memory->cpu);
					}
					break;
				}
			}


			chan->bits_left--;
			bit_pos = 7 - (chan->bits_left & 7);
			if (7 == bit_pos)
			{
				//chan->cur_byte = info->APU.dpcm.memory->read_byte(chan->address);
				chan->cur_byte = info->APU.dpcm.memory[chan->address];
				chan->address++;
				// On overflow, the address is set to 8000
				if (chan->address >= 0x10000)
					chan->address -= 0x8000;
				chan->length--;
			}

			if (chan->cur_byte & (1 << bit_pos))
//				chan->regs[1] += 2;
				chan->vol += 2; /* FIXED * DPCM channel only uses the upper 6 bits of the DAC */
			else
//				chan->regs[1] -= 2;
				chan->vol -= 2;
		}
	}

	if (! DPCMBase0)
	{
		if (chan->vol > 63)
			chan->vol = 63;
		else if (chan->vol < -64)
			chan->vol = -64;
	}
	else
	{
		if (chan->vol > 127)
			chan->vol = 127;
		else if (chan->vol < 0)
			chan->vol = 0;
	}
	chan->output = (int8)chan->vol;
}

/* WRITE REGISTER VALUE */
INLINE void apu_regwrite(nesapu_state *info, int address, uint8 value)
{
	int chan = (address & 4) ? 1 : 0;

	switch (address)
	{
	/* squares */
	case APU_WRA0:
	case APU_WRB0:
		info->APU.squ[chan].regs[0] = value;
		break;

	case APU_WRA1:
	case APU_WRB1:
		info->APU.squ[chan].regs[1] = value;
		break;

	case APU_WRA2:
	case APU_WRB2:
		info->APU.squ[chan].regs[2] = value;
		if (info->APU.squ[chan].enabled)
			info->APU.squ[chan].freq = ((((info->APU.squ[chan].regs[3] & 7) << 8) + value) + 1) << 16;
		break;

	case APU_WRA3:
	case APU_WRB3:
		info->APU.squ[chan].regs[3] = value;

		if (info->APU.squ[chan].enabled)
		{
		// TODO: Test, if it sounds better with or without it.
		//	info->APU.squ[chan].adder = 0;	// Thanks to Delek
			info->APU.squ[chan].vbl_length = info->vbl_times[value >> 3];
			info->APU.squ[chan].env_vol = 0;
			info->APU.squ[chan].freq = ((((value & 7) << 8) + info->APU.squ[chan].regs[2]) + 1) << 16;
		}

		break;

	/* triangle */
	case APU_WRC0:
		info->APU.tri.regs[0] = value;

		if (info->APU.tri.enabled)
		{                                          /* ??? */
			if (false == info->APU.tri.counter_started)
				info->APU.tri.linear_length = info->sync_times2[value & 0x7f];
		}

		break;

	//case 0x4009:
	case APU_WRC1:
		/* unused */
		info->APU.tri.regs[1] = value;
		break;

	case APU_WRC2:
		info->APU.tri.regs[2] = value;
		break;

	case APU_WRC3:
		info->APU.tri.regs[3] = value;

		/* this is somewhat of a hack.  there is some latency on the Real
		** Thing between when trireg0 is written to and when the linear
		** length counter actually begins its countdown.  we want to prevent
		** the case where the program writes to the freq regs first, then
		** to reg 0, and the counter accidentally starts running because of
		** the sound queue's timestamp processing.
		**
		** set to a few NES sample -- should be sufficient
		**
		**    3 * (1789772.727 / 44100) = ~122 cycles, just around one scanline
		**
		** should be plenty of time for the 6502 code to do a couple of table
		** dereferences and load up the other triregs
		*/

	/* used to be 3, but now we run the clock faster, so base it on samples/sync */
		info->APU.tri.write_latency = (info->samps_per_sync + 239) / 240;

		if (info->APU.tri.enabled)
		{
			info->APU.tri.counter_started = false;
			info->APU.tri.vbl_length = info->vbl_times[value >> 3];
			info->APU.tri.linear_length = info->sync_times2[info->APU.tri.regs[0] & 0x7f];
		}

		break;

	/* noise */
	case APU_WRD0:
		info->APU.noi.regs[0] = value;
		break;

	case 0x400D:
		/* unused */
		info->APU.noi.regs[1] = value;
		break;

	case APU_WRD2:
		info->APU.noi.regs[2] = value;
		break;

	case APU_WRD3:
		info->APU.noi.regs[3] = value;

		if (info->APU.noi.enabled)
		{
			info->APU.noi.vbl_length = info->vbl_times[value >> 3];
			info->APU.noi.env_vol = 0; /* reset envelope */
		}
		break;

	/* DMC */
	case APU_WRE0:
		info->APU.dpcm.regs[0] = value;
		if (!(value & 0x80)) {
			//downcast<n2a03_device &>(m_APU.dpcm.memory->device()).set_input_line(N2A03_APU_IRQ_LINE, CLEAR_LINE);
			info->APU.dpcm.irq_occurred = false;
		}
		break;

	case APU_WRE1: /* 7-bit DAC */
		info->APU.dpcm.regs[1] = value & 0x7f;
		if (!DPCMBase0)
			info->APU.dpcm.vol = (info->APU.dpcm.regs[1]-64);
		else
			info->APU.dpcm.vol = info->APU.dpcm.regs[1];
		break;

	case APU_WRE2:
		info->APU.dpcm.regs[2] = value;
		//apu_dpcmreset(info->APU.dpcm);
		break;

	case APU_WRE3:
		info->APU.dpcm.regs[3] = value;
		break;

	case APU_IRQCTRL:
		if(value & 0x80)
			info->APU.step_mode = 5;
		else
			info->APU.step_mode = 4;
		break;

	case APU_SMASK:
		if (value & 0x01)
			info->APU.squ[0].enabled = true;
		else
		{
			info->APU.squ[0].enabled = false;
			info->APU.squ[0].vbl_length = 0;
		}

		if (value & 0x02)
			info->APU.squ[1].enabled = true;
		else
		{
			info->APU.squ[1].enabled = false;
			info->APU.squ[1].vbl_length = 0;
		}

		if (value & 0x04)
			info->APU.tri.enabled = true;
		else
		{
			info->APU.tri.enabled = false;
			info->APU.tri.vbl_length = 0;
			info->APU.tri.linear_length = 0;
			info->APU.tri.counter_started = false;
			info->APU.tri.write_latency = 0;
		}

		if (value & 0x08)
			info->APU.noi.enabled = true;
		else
		{
			info->APU.noi.enabled = false;
			info->APU.noi.vbl_length = 0;
		}

		if (value & 0x10)
		{
			/* only reset dpcm values if DMA is finished */
			if (!info->APU.dpcm.enabled)
			{
				info->APU.dpcm.enabled = true;
				apu_dpcmreset(&info->APU.dpcm);
			}
		}
		else
			info->APU.dpcm.enabled = false;

		info->APU.dpcm.irq_occurred = false;

		break;
	default:
#ifdef MAME_DEBUG
logerror("invalid apu write: $%02X at $%04X\n", value, address);
#endif
		break;
	}
}

/* UPDATE SOUND BUFFER USING CURRENT DATA */
void nes_apu_update(void* chip, UINT32 samples, DEV_SMPL **outputs)
{
	nesapu_state *info = (nesapu_state*)chip;
	apu_t *apu = &info->APU;
	DEV_SMPL* bufL = outputs[0];
	DEV_SMPL* bufR = outputs[1];
	UINT32 i;

	for (i = 0; i < samples; i++)
	{
		INT16 squ1, squ2, tri, noi, dpcm;

		apu_square(info, &apu->squ[0]);
		apu_square(info, &apu->squ[1]);
		apu_triangle(info, &apu->tri);
		apu_noise(info, &apu->noi);
		apu_dpcm(info, &apu->dpcm);

		if (info->nonlinear_mixing)
		{
			squ1 = (apu->squ[0].output >= 0) ? apu->squ[0].output : 0;
			squ2 = (apu->squ[1].output >= 0) ? apu->squ[1].output : 0;
			tri  = (apu->tri.output + 0x10) / 2;
			noi  = (apu->noi.output >= 0) ? apu->noi.output : 0;
			dpcm = apu->dpcm.output;
			bufL[i] = square_lut[squ1 + squ2];
			bufL[i] += tnd_lut[tri][noi][dpcm];
			bufR[i] = bufL[i];
		}
		else
		{
			// These volumes should match NSFPlay's NES core better
			squ1 = apu->squ[0].output * 0x100;	// [-15..+15] << 8 * 1.0
			squ2 = apu->squ[1].output * 0x100;	// [-15..+15] << 8 * 1.0
			tri  = apu->tri.output * 0xC0;	// [-16..+16] << 8 * 0.75
			noi  = apu->noi.output * 0xC0;	// [-15..+15] << 8 * 0.75
			dpcm = apu->dpcm.output * 0xC0;	// [0..+127] << 8 * 0.75

			bufL[i]  = APPLY_PANNING_S(squ1, apu->squ[0].Pan[0]);
			bufR[i]  = APPLY_PANNING_S(squ1, apu->squ[0].Pan[1]);
			bufL[i] += APPLY_PANNING_S(squ2, apu->squ[1].Pan[0]);
			bufR[i] += APPLY_PANNING_S(squ2, apu->squ[1].Pan[1]);
			bufL[i] += APPLY_PANNING_S(tri, apu->tri.Pan[0]);
			bufR[i] += APPLY_PANNING_S(tri, apu->tri.Pan[1]);
			bufL[i] += APPLY_PANNING_S(noi, apu->noi.Pan[0]);
			bufR[i] += APPLY_PANNING_S(noi, apu->noi.Pan[1]);
			bufL[i] += APPLY_PANNING_L(dpcm, apu->dpcm.Pan[0]);	// could be 0..24384, thus use _L macro
			bufR[i] += APPLY_PANNING_L(dpcm, apu->dpcm.Pan[1]);
		}
	}
}

/* READ VALUES FROM REGISTERS */
uint8 nes_apu_read(void* chip, uint8 address)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	if (address == 0x15) /*FIXED* Address $4015 has different behaviour*/
	{
		uint8 readval = 0;
		if (info->APU.squ[0].vbl_length > 0)
			readval |= 0x01;

		if (info->APU.squ[1].vbl_length > 0)
			readval |= 0x02;

		if (info->APU.tri.vbl_length > 0)
			readval |= 0x04;

		if (info->APU.noi.vbl_length > 0)
			readval |= 0x08;

		if (info->APU.dpcm.enabled == true)
			readval |= 0x10;

		if (info->APU.dpcm.irq_occurred == true)
			readval |= 0x80;

		return readval;
	}
	else
		return info->APU.regs[address];
}

/* WRITE VALUE TO TEMP REGISTRY AND QUEUE EVENT */
void nes_apu_write(void *chip,uint8 address, uint8 value)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	if (address >= 0x20)
		return;
	info->APU.regs[address]=value;
	apu_regwrite(info,address,value);
}

/* INITIALIZE APU SYSTEM */
#define SCREEN_HZ	60
void* device_start_nesapu(UINT32 clock, UINT32 rate)
{
	nesapu_state *info;

	info = (nesapu_state*)calloc(1, sizeof(nesapu_state));
	if (info == NULL)
		return NULL;
	
	/* Initialize global variables */
	info->samps_per_sync = rate / SCREEN_HZ;
	info->buffer_size = info->samps_per_sync;
	info->real_rate = info->samps_per_sync * SCREEN_HZ;
	info->apu_incsize = (float) (clock / (float) info->real_rate);
	info->is_pal = (clock >= PAL_APU_CLOCK - 10 && clock <= PAL_APU_CLOCK + 10);
	info->nonlinear_mixing = 0;

	/* Use initializer calls */
	create_mixer_lut();
	create_vbltimes(info->vbl_times,vbl_length,info->samps_per_sync);
	create_syncs(info, info->samps_per_sync);

	/* Adjust buffer size if 16 bits */
	info->buffer_size+=info->samps_per_sync;

	info->APU.dpcm.memory = NULL;

	nesapu_set_mute_mask(info, 0x00);

	Panning_Centre(info->APU.squ[0].Pan);
	Panning_Centre(info->APU.squ[1].Pan);
	Panning_Centre(info->APU.tri.Pan);
	Panning_Centre(info->APU.noi.Pan);
	Panning_Centre(info->APU.dpcm.Pan);

	return info;
}

void device_stop_nesapu(void* chip)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	free(info);
	
	return;
}

void device_reset_nesapu(void* chip)
{
	nesapu_state *info = (nesapu_state*)chip;
	const UINT8* MemPtr;
	UINT32 MuteMask;
	INT32* PanPtrs[5] = {info->APU.squ[0].Pan, info->APU.squ[1].Pan, info->APU.tri.Pan, info->APU.noi.Pan, info->APU.dpcm.Pan};
	INT32 PanBak[5][2];
	UINT8 CurChn;
	UINT8 CurReg;
	
	MemPtr = info->APU.dpcm.memory;
	MuteMask = nesapu_get_mute_mask(info);
	for (CurChn = 0; CurChn < 5; CurChn ++)
	{
		PanBak[CurChn][0] = PanPtrs[CurChn][0];
		PanBak[CurChn][1] = PanPtrs[CurChn][1];
	}
	memset(&info->APU, 0x00, sizeof(apu_t));
	info->APU.dpcm.memory = MemPtr;
	apu_dpcmreset(&info->APU.dpcm);
	nesapu_set_mute_mask(info, MuteMask);
	for (CurChn = 0; CurChn < 5; CurChn ++)
	{
		PanPtrs[CurChn][0] = PanBak[CurChn][0];
		PanPtrs[CurChn][1] = PanBak[CurChn][1];
	}

	info->APU.noi.seed = 1;
	for (CurReg = 0x00; CurReg < 0x18; CurReg ++)
		nes_apu_write(info, CurReg, 0x00);
	
	nes_apu_write(info, 0x15, 0x00);
	nes_apu_write(info, 0x15, 0x0F);
	
	return;
}

void nesapu_set_rom(void* chip, const UINT8* ROMData)
{
	nesapu_state *info = (nesapu_state*)chip;
	info->APU.dpcm.memory = ROMData;
	
	return;
}

void nesapu_set_mute_mask(void* chip, UINT32 MuteMask)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	info->APU.squ[0].Muted = (MuteMask >> 0) & 0x01;
	info->APU.squ[1].Muted = (MuteMask >> 1) & 0x01;
	info->APU.tri.Muted = (MuteMask >> 2) & 0x01;
	info->APU.noi.Muted = (MuteMask >> 3) & 0x01;
	info->APU.dpcm.Muted = (MuteMask >> 4) & 0x01;
	
	return;
}

UINT32 nesapu_get_mute_mask(void* chip)
{
	nesapu_state *info = (nesapu_state*)chip;
	UINT32 muteMask;
	
	muteMask =	(info->APU.squ[0].Muted << 0) |
				(info->APU.squ[1].Muted << 1) |
				(info->APU.tri.Muted << 2) |
				(info->APU.noi.Muted << 3) |
				(info->APU.dpcm.Muted << 4);
	
	return muteMask;
}

void nesapu_set_options(void *chip, UINT32 Flags)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	info->nonlinear_mixing = (Flags >> 1) & 0x01;
}

void nesapu_set_panning(void* chip, INT16 square1, INT16 square2, INT16 triangle, INT16 noise, INT16 dpcm)
{
	nesapu_state *info = (nesapu_state*)chip;
	
	Panning_Calculate(info->APU.squ[0].Pan, square1);
	Panning_Calculate(info->APU.squ[1].Pan, square2);
	Panning_Calculate(info->APU.tri.Pan, triangle);
	Panning_Calculate(info->APU.noi.Pan, noise);
	Panning_Calculate(info->APU.dpcm.Pan, dpcm);
	
	return;
}
