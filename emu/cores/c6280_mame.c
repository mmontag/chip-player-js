// license:BSD-3-Clause
// copyright-holders:Charles MacDonald
/*
    HuC6280 sound chip emulator
    by Charles MacDonald
    E-mail: cgfm2@hotmail.com
    WWW: http://cgfm2.emuviews.com

    Thanks to:

    - Paul Clifford for his PSG documentation.
    - Richard Bannister for the TGEmu-specific sound updating code.
    - http://www.uspto.gov for the PSG patents.
    - All contributors to the tghack-list.

    Changes:

    (03/30/2003)
    - Removed TGEmu specific code and added support functions for MAME.
    - Modified setup code to handle multiple chips with different clock and
      volume settings.

    Missing features / things to do:

    - Verify LFO frequency from real hardware.

    - Add shared index for waveform playback and sample writes. Almost every
      game will reset the index prior to playback so this isn't an issue.

    - While the noise emulation is complete, the data for the pseudo-random
      bitstream is calculated by machine().rand() and is not a representation of what
      the actual hardware does.

    For some background on Hudson Soft's C62 chipset:

    - http://www.hudsonsoft.net/ww/about/about.html
    - http://www.hudson.co.jp/corp/eng/coinfo/history.html

*/

#include <stdlib.h>	// for rand()
#include <string.h>	// for memset()
#include <math.h>	// for pow()

#include "../../stdtype.h"
#include "../snddef.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../EmuHelper.h"
#include "c6280_mame.h"


static void c6280mame_w(void *chip, UINT8 offset, UINT8 data);
static UINT8 c6280mame_r(void* chip, UINT8 offset);

static void c6280mame_update(void* param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_c6280_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void* device_start_c6280mame(UINT32 clock, UINT32 rate);
static void device_stop_c6280mame(void* chip);
static void device_reset_c6280mame(void* chip);

static void c6280mame_set_mute_mask(void* chip, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, c6280mame_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, c6280mame_r},
	{0x00, 0x00, 0, NULL}
};
DEV_DEF devDef_C6280_MAME =
{
	"HuC6280", "MAME", FCC_MAME,
	
	device_start_c6280_mame,
	device_stop_c6280mame,
	device_reset_c6280mame,
	c6280mame_update,
	
	NULL,	// SetOptionBits
	c6280mame_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};


typedef struct {
	UINT16 frequency;
	UINT8 control;
	UINT8 balance;
	UINT8 waveform[32];
	UINT8 index;
	INT16 dda;
	UINT8 noise_control;
	UINT32 noise_counter;
	UINT32 counter;
	UINT8 Muted;
} t_channel;

typedef struct {
	DEV_DATA _devData;
	
	UINT8 select;
	UINT8 balance;
	UINT8 lfo_frequency;
	UINT8 lfo_control;
	t_channel channel[8];	// is 8, because: p->select = data & 0x07;
	INT16 volume_table[32];
	UINT32 noise_freq_tab[32];
	UINT32 wave_freq_tab[4096];
} c6280_t;


//-------------------------------------------------
//  calculate_clocks - (re)calculate clock-derived
//  members
//-------------------------------------------------

static void c6280_calculate_clocks(c6280_t *p, double clk, double rate)
{
	int i;
	double step;
	//double rate = clock / 16;

	/* Make waveform frequency table */
	for (i = 0; i < 4096; i += 1)
	{
		step = ((clk / rate) * 4096) / (i+1);
		p->wave_freq_tab[(1 + i) & 0xFFF] = (UINT32)step;
	}

	/* Make noise frequency table */
	for (i = 0; i < 32; i += 1)
	{
		step = ((clk / rate) * 32) / (i+1);
		p->noise_freq_tab[i] = (UINT32)step;
	}
}


static void c6280mame_w(void *chip, UINT8 offset, UINT8 data)
{
	c6280_t *p = (c6280_t *)chip;
	t_channel *chan = &p->channel[p->select];

	//m_cpudevice->io_set_buffer(data);

	switch(offset & 0x0F)
	{
		case 0x00: /* Channel select */
			p->select = data & 0x07;
			break;

		case 0x01: /* Global balance */
			p->balance  = data;
			break;

		case 0x02: /* Channel frequency (LSB) */
			chan->frequency = (chan->frequency & 0x0F00) | data;
			chan->frequency &= 0x0FFF;
			break;

		case 0x03: /* Channel frequency (MSB) */
			chan->frequency = (chan->frequency & 0x00FF) | (data << 8);
			chan->frequency &= 0x0FFF;
			break;

		case 0x04: /* Channel control (key-on, DDA mode, volume) */

			/* 1-to-0 transition of DDA bit resets waveform index */
			if((chan->control & 0x40) && ((data & 0x40) == 0))
			{
				chan->index = 0;
			}
			chan->control = data;
			break;

		case 0x05: /* Channel balance */
			chan->balance = data;
			break;

		case 0x06: /* Channel waveform data */

			switch(chan->control & 0xC0)
			{
				case 0x00:
				case 0x80:
					chan->waveform[chan->index & 0x1F] = data & 0x1F;
					chan->index = (chan->index + 1) & 0x1F;
					break;

				case 0x40:
					break;

				case 0xC0:
					chan->dda = data & 0x1F;
					break;
			}

			break;

		case 0x07: /* Noise control (enable, frequency) */
			chan->noise_control = data;
			break;

		case 0x08: /* LFO frequency */
			p->lfo_frequency = data;
			break;

		case 0x09: /* LFO control (enable, mode) */
			p->lfo_control = data;
			break;

		default:
			break;
	}
}


static void c6280mame_update(void* param, UINT32 samples, DEV_SMPL **outputs)
{
	static const UINT8 scale_tab[] = {
		0x00, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F,
		0x10, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F
	};
	int ch;
	UINT32 i;
	c6280_t *p = (c6280_t *)param;

	UINT8 lmal = (p->balance >> 4) & 0x0F;
	UINT8 rmal = (p->balance >> 0) & 0x0F;

	lmal = scale_tab[lmal];
	rmal = scale_tab[rmal];

	/* Clear buffer */
	memset(outputs[0], 0x00, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0x00, samples * sizeof(DEV_SMPL));

	for (ch = 0; ch < 6; ch++)
	{
		/* Only look at enabled channels */
		if((p->channel[ch].control & 0x80) && ! p->channel[ch].Muted)
		{
			UINT8 lal = (p->channel[ch].balance >> 4) & 0x0F;
			UINT8 ral = (p->channel[ch].balance >> 0) & 0x0F;
			UINT8 al  = p->channel[ch].control & 0x1F;
			INT16 vll, vlr;

			lal = scale_tab[lal];
			ral = scale_tab[ral];

			/* Calculate volume just as the patent says */
			vll = (0x1F - lal) + (0x1F - al) + (0x1F - lmal);
			if(vll > 0x1F) vll = 0x1F;

			vlr = (0x1F - ral) + (0x1F - al) + (0x1F - rmal);
			if(vlr > 0x1F) vlr = 0x1F;

			vll = p->volume_table[vll];
			vlr = p->volume_table[vlr];

			/* Check channel mode */
			if((ch >= 4) && (p->channel[ch].noise_control & 0x80))
			{
				/* Noise mode */
				UINT32 step = p->noise_freq_tab[(p->channel[ch].noise_control & 0x1F) ^ 0x1F];
				for(i = 0; i < samples; i += 1)
				{
					static int data = 0;
					p->channel[ch].noise_counter += step;
					if(p->channel[ch].noise_counter >= 0x800)
					{
						data = (rand() & 1) ? 0x1F : 0;
					}
					p->channel[ch].noise_counter &= 0x7FF;
					outputs[0][i] += (vll * (data - 16));
					outputs[1][i] += (vlr * (data - 16));
				}
			}
			else
			if(p->channel[ch].control & 0x40)
			{
				/* DDA mode */
				for(i = 0; i < samples; i++)
				{
					outputs[0][i] += (vll * (p->channel[ch].dda - 16));
					outputs[1][i] += (vlr * (p->channel[ch].dda - 16));
				}
			}
			else
			{
				if ((p->lfo_control & 0x80) && (ch < 2))
				{
					if (ch == 0) // CH 0 only, CH 1 is muted
					{
						/* Waveform mode with LFO */
						UINT32 step = p->channel[0].frequency;
						UINT16 lfo_step = p->channel[1].frequency;
						for (i = 0; i < samples; i += 1)
						{
							int offset, lfooffset;
							INT16 data, lfo_data;
							lfooffset = (p->channel[1].counter >> 12) & 0x1F;
							p->channel[1].counter += p->wave_freq_tab[(lfo_step * p->lfo_frequency) & 0xfff]; // TODO : multiply? verify this from real hardware.
							p->channel[1].counter &= 0x1FFFF;
							lfo_data = p->channel[1].waveform[lfooffset];
							if (p->lfo_control & 3)
								step += ((lfo_data - 16) << (((p->lfo_control & 3)-1)<<1)); // verified from patent, TODO : same in real hardware?

							offset = (p->channel[0].counter >> 12) & 0x1F;
							p->channel[0].counter += p->wave_freq_tab[step & 0xfff];
							p->channel[0].counter &= 0x1FFFF;
							data = p->channel[0].waveform[offset];
							outputs[0][i] += (vll * (data - 16));
							outputs[1][i] += (vlr * (data - 16));
						}
					}
				}
				else
				{
					/* Waveform mode */
					UINT32 step = p->wave_freq_tab[p->channel[ch].frequency];
					for (i = 0; i < samples; i += 1)
					{
						int offset;
						INT16 data;
						offset = (p->channel[ch].counter >> 12) & 0x1F;
						p->channel[ch].counter += step;
						p->channel[ch].counter &= 0x1FFFF;
						data = p->channel[ch].waveform[offset];
						outputs[0][i] += (vll * (data - 16));
						outputs[1][i] += (vlr * (data - 16));
					}
				}
			}
		}
	}
}


/*--------------------------------------------------------------------------*/
/* MAME specific code                                                       */
/*--------------------------------------------------------------------------*/

static UINT8 device_start_c6280_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 16;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	chip = device_start_c6280mame(cfg->clock, rate);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_C6280_MAME);
	return 0x00;
}

static void* device_start_c6280mame(UINT32 clock, UINT32 rate)
{
	c6280_t *info;
	int i;
	double step;
	/* Loudest volume level for table */
	double level = 65536.0 / 6.0 / 32.0;

	info = (c6280_t*)calloc(1, sizeof(c6280_t));
	if (info == NULL)
		return NULL;

	c6280_calculate_clocks(info, clock, rate);

	/* Make volume table */
	/* PSG has 48dB volume range spread over 32 steps */
	step = 48.0 / 32.0;
	for (i = 0; i < 31; i++)
	{
		info->volume_table[i] = (UINT16)level;
		level /= pow(10.0, step / 20.0);
	}
	info->volume_table[31] = 0;

	c6280mame_set_mute_mask(info, 0x00);

	return info;
}

static void device_stop_c6280mame(void* chip)
{
	c6280_t *info = (c6280_t *)chip;
	
	free(info);
	
	return;
}

static void device_reset_c6280mame(void* chip)
{
	c6280_t *info = (c6280_t *)chip;
	UINT8 CurChn;
	t_channel* TempChn;
	
	info->select = 0x00;
	info->balance = 0x00;
	info->lfo_frequency = 0x00;
	info->lfo_control = 0x00;
	
	for (CurChn = 0; CurChn < 6; CurChn ++)
	{
		TempChn = &info->channel[CurChn];
		
		TempChn->frequency = 0x00;
		TempChn->control = 0x00;
		TempChn->balance = 0x00;
		memset(TempChn->waveform, 0x00, 0x20);
		TempChn->index = 0x00;
		TempChn->dda = 0x00;
		TempChn->noise_control = 0x00;
		TempChn->noise_counter = 0x00;
		TempChn->counter = 0x00;
	}
	
	return;
}

static UINT8 c6280mame_r(void* chip, UINT8 offset)
{
	c6280_t *info = (c6280_t *)chip;
	//return m_cpudevice->io_get_buffer();
	if (offset == 0)
		return info->select;
	return 0;
}


static void c6280mame_set_mute_mask(void* chip, UINT32 MuteMask)
{
	c6280_t *info = (c6280_t *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 6; CurChn ++)
		info->channel[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
