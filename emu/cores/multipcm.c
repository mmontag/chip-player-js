// license:BSD-3-Clause
// copyright-holders:Miguel Angel Horna
/*
 * Yamaha YMW-258-F 'GEW8' (aka Sega 315-5560) emulation.
 *
 * by Miguel Angel Horna (ElSemi) for Model 2 Emulator and MAME.
 * Information by R. Belmont and the YMF278B (OPL4) manual.
 *
 * voice registers:
 * 0: Pan
 * 1: Index of sample
 * 2: LSB of pitch (low 2 bits seem unused so)
 * 3: MSB of pitch (ooooppppppppppxx) (o=octave (4 bit signed), p=pitch (10 bits), x=unused?
 * 4: voice control: top bit = 1 for key on, 0 for key off
 * 5: bit 0: 0: interpolate volume changes, 1: direct set volume,
 *    bits 1-7 = volume attenuate (0=max, 7f=min)
 * 6: LFO frequency + Phase LFO depth
 * 7: Amplitude LFO size
 *
 * The first sample ROM contains a variable length metadata table with 12
 * bytes per instrument sample. This is very similar to the YMF278B 'OPL4'.
 * This sample format might be derived from the one used by the older YM7138 'GEW6' chip.
 *
 * The first 3 bytes are the offset into the file (big endian). (0, 1, 2).
 * Bit 23 is the sample format flag: 0 for 8-bit linear, 1 for 12-bit linear.
 * Bits 21 and 22 are used by the MU5 on some samples for as-yet unknown purposes.
 * The next 2 are the loop start point, in samples (big endian) (3, 4)
 * The next 2 are the 2's complement negation of of the total number of samples (big endian) (5, 6)
 * The next byte is LFO freq + depth (copied to reg 6 ?) (7, 8)
 * The next 3 are envelope params (Attack, Decay1 and 2, sustain level, release, Key Rate Scaling) (9, 10, 11)
 * The next byte is Amplitude LFO size (copied to reg 7 ?)
 *
 * TODO
 * - http://dtech.lv/techarticles_yamaha_chips.html indicates FM support, which we don't have yet.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "multipcm.h"

static void MultiPCM_update(void *info, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_multipcm(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_multipcm(void *info);
static void device_reset_multipcm(void *info);

static void multipcm_write(void *info, UINT8 offset, UINT8 data);
static void multipcm_w_quick(void *info, UINT8 offset, UINT8 data);
static UINT8 multipcm_r(void *info, UINT8 offset);

static void multipcm_alloc_rom(void* info, UINT32 memsize);
static void multipcm_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data);

static void multipcm_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, multipcm_write},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D8, 0, multipcm_w_quick},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, multipcm_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, multipcm_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, multipcm_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, multipcm_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"YMW258", "MAME", FCC_MAME,
	
	device_start_multipcm,
	device_stop_multipcm,
	device_reset_multipcm,
	MultiPCM_update,
	
	NULL,	// SetOptionBits
	multipcm_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_YMW258[] =
{
	&devDef,
	NULL
};


#define MULTIPCM_CLOCKDIV   224.0f

typedef struct
{
	UINT32 start;
	UINT32 loop;
	UINT32 end;
	UINT8 attack_reg;
	UINT8 decay1_reg;
	UINT8 decay2_reg;
	UINT8 decay_level;
	UINT8 release_reg;
	UINT8 key_rate_scale;
	UINT8 lfo_vibrato_reg;
	UINT8 lfo_amplitude_reg;
	UINT8 format;
} sample_t;

typedef enum
{
	ATTACK,
	DECAY1,
	DECAY2,
	RELEASE
} state_t;

typedef struct
{
	INT32 volume;
	state_t state;
	INT32 step;
	//step vals
	INT32 attack_rate;  // Attack
	INT32 decay1_rate;  // Decay1
	INT32 decay2_rate;  // Decay2
	INT32 release_rate; // Release
	INT32 decay_level;  // Decay level
} envelope_gen_t;

typedef struct
{
	UINT16 phase;
	UINT32 phase_step;
	INT32 *table;
	INT32 *scale;
} lfo_t;

typedef struct
{
	UINT8 slot_index;
	UINT8 regs[8];
	UINT8 playing;
	sample_t sample;
	UINT32 base;
	UINT32 offset;
	UINT32 step;
	UINT32 pan;
	UINT32 total_level;
	UINT32 dest_total_level;
	INT32 total_level_step;
	INT32 prev_sample;
	envelope_gen_t envelope_gen;
	lfo_t pitch_lfo; // Pitch lfo
	lfo_t amplitude_lfo; // AM lfo
	UINT8 format;
	
	UINT8 muted;
} slot_t;

typedef struct _MultiPCM MultiPCM;
struct _MultiPCM
{
	DEV_DATA _devData;
	
	// internal state
	slot_t slots[28];
	INT32 cur_slot;
	INT32 address;
	UINT8 sega_banking;
	UINT32 bank0;
	UINT32 bank1;
	float rate;

	UINT32 attack_step[0x40];
	UINT32 decay_release_step[0x40];    // Envelope step tables
	UINT32 freq_step_table[0x400];      // Frequency step table

	//INT32 left_pan_table[0x800];
	//INT32 right_pan_table[0x800];
	//INT32 linear_to_exp_volume[0x400];
	INT32 total_level_steps[2];

	//INT32 pitch_table[256];
	//INT32 pitch_scale_tables[8][256];
	//INT32 amplitude_table[256];
	//INT32 amplitude_scale_tables[8][256];
	
	UINT32 ROMMask;
	UINT32 ROMSize;
	UINT8 *ROM;
};


static UINT8 IsInit = 0;

static INT32 left_pan_table[0x800];
static INT32 right_pan_table[0x800];

static const INT32 VALUE_TO_CHANNEL[32] =
{
	0, 1, 2, 3, 4, 5, 6 , -1,
	7, 8, 9, 10,11,12,13, -1,
	14,15,16,17,18,19,20, -1,
	21,22,23,24,25,26,27, -1,
};


/*******************************
        ENVELOPE SECTION
*******************************/

//Times are based on a 44100Hz timebase. It's adjusted to the actual sampling rate on startup

static const double BASE_TIMES[64] = {
	0,          0,          0,          0,
	6222.95,    4978.37,    4148.66,    3556.01,
	3111.47,    2489.21,    2074.33,    1778.00,
	1555.74,    1244.63,    1037.19,    889.02,
	777.87,     622.31,     518.59,     444.54,
	388.93,     311.16,     259.32,     222.27,
	194.47,     155.60,     129.66,     111.16,
	97.23,      77.82,      64.85,      55.60,
	48.62,      38.91,      32.43,      27.80,
	24.31,      19.46,      16.24,      13.92,
	12.15,      9.75,       8.12,       6.98,
	6.08,       4.90,       4.08,       3.49,
	3.04,       2.49,       2.13,       1.90,
	1.72,       1.41,       1.18,       1.04,
	0.91,       0.73,       0.59,       0.50,
	0.45,       0.45,       0.45,       0.45
};

#define attack_rate_to_decay_rate   14.32833
static INT32 linear_to_exp_volume[0x400];

#define TL_SHIFT    12
#define EG_SHIFT    16

static void init_sample(MultiPCM *ptChip, sample_t *sample, UINT16 index)
{
	UINT32 address = (index * 12) & ptChip->ROMMask;

	sample->start = (ptChip->ROM[address + 0] << 16) | (ptChip->ROM[address + 1] << 8) | ptChip->ROM[address + 2];
	sample->format = (sample->start>>20) & 0xfe;
	sample->start &= 0x3fffff;
	sample->loop = (ptChip->ROM[address + 3] << 8) | ptChip->ROM[address + 4];
	sample->end = 0xffff - ((ptChip->ROM[address + 5] << 8) | ptChip->ROM[address + 6]);
	sample->attack_reg = (ptChip->ROM[address + 8] >> 4) & 0xf;
	sample->decay1_reg = ptChip->ROM[address + 8] & 0xf;
	sample->decay2_reg = ptChip->ROM[address + 9] & 0xf;
	sample->decay_level = (ptChip->ROM[address + 9] >> 4) & 0xf;
	sample->release_reg = ptChip->ROM[address + 10] & 0xf;
	sample->key_rate_scale = (ptChip->ROM[address + 10] >> 4) & 0xf;
	sample->lfo_vibrato_reg = ptChip->ROM[address + 7];
	sample->lfo_amplitude_reg = ptChip->ROM[address + 11] & 0xf;
}

static INT32 envelope_generator_update(slot_t *slot)
{
	switch(slot->envelope_gen.state)
	{
		case ATTACK:
			slot->envelope_gen.volume += slot->envelope_gen.attack_rate;
			if (slot->envelope_gen.volume >= (0x3ff << EG_SHIFT))
			{
				slot->envelope_gen.state = DECAY1;
				if (slot->envelope_gen.decay1_rate >= (0x400 << EG_SHIFT)) //Skip DECAY1, go directly to DECAY2
					slot->envelope_gen.state = DECAY2;
				slot->envelope_gen.volume = 0x3ff << EG_SHIFT;
			}
			break;
		case DECAY1:
			slot->envelope_gen.volume -= slot->envelope_gen.decay1_rate;
			if (slot->envelope_gen.volume <= 0)
				slot->envelope_gen.volume = 0;
			if (slot->envelope_gen.volume >> EG_SHIFT <= (slot->envelope_gen.decay_level << 6))
				slot->envelope_gen.state = DECAY2;
			break;
		case DECAY2:
			slot->envelope_gen.volume -= slot->envelope_gen.decay2_rate;
			if (slot->envelope_gen.volume <= 0)
				slot->envelope_gen.volume = 0;
			break;
		case RELEASE:
			slot->envelope_gen.volume -= slot->envelope_gen.release_rate;
			if (slot->envelope_gen.volume <= 0)
			{
				slot->envelope_gen.volume = 0;
				slot->playing = 0;
			}
			break;
		default:
			return 1 << TL_SHIFT;
	}

	return linear_to_exp_volume[slot->envelope_gen.volume >> EG_SHIFT];
}

INLINE UINT32 get_rate(const UINT32 *steps, UINT32 rate, UINT32 val)
{
	INT32 r = 4 * val + rate;
	if (val == 0)
		return steps[0];
	if (val == 0xf)
		return steps[0x3f];
	if (r > 0x3f)
		r = 0x3f;
	return steps[r];
}

static void envelope_generator_calc(MultiPCM *ptChip, slot_t *slot)
{
	INT32 octave = ((slot->regs[3] >> 4) - 1) & 0xf;
	INT32 rate;
	if (octave & 8)
		octave = octave - 16;

	if (slot->sample.key_rate_scale != 0xf)
		rate = (octave + slot->sample.key_rate_scale) * 2 + ((slot->regs[3] >> 3) & 1);
	else
		rate = 0;

	slot->envelope_gen.attack_rate = get_rate(ptChip->attack_step, rate, slot->sample.attack_reg);
	slot->envelope_gen.decay1_rate = get_rate(ptChip->decay_release_step, rate, slot->sample.decay1_reg);
	slot->envelope_gen.decay2_rate = get_rate(ptChip->decay_release_step, rate, slot->sample.decay2_reg);
	slot->envelope_gen.release_rate = get_rate(ptChip->decay_release_step, rate, slot->sample.release_reg);
	slot->envelope_gen.decay_level = 0xf - slot->sample.decay_level;

}

/*****************************
        LFO  SECTION
*****************************/

#define LFO_SHIFT   8

static INT32 pitch_table[256];
static INT32 amplitude_table[256];

static const float LFO_FREQ[8] = // In Hertz
{
	0.168f,
	2.019f,
	3.196f,
	4.206f,
	5.215f,
	5.888f,
	6.224f,
	7.066f
};

static const float PHASE_SCALE_LIMIT[8] = // In Cents
{
	0.0f,
	3.378f,
	5.065f,
	6.750f,
	10.114f,
	20.170f,
	40.180f,
	79.307f
};

static const float AMPLITUDE_SCALE_LIMIT[8] = // In Decibels
{
	0.0f,
	0.4f,
	0.8f,
	1.5f,
	3.0f,
	6.0f,
	12.0f,
	24.0f
};

static INT32 pitch_scale_tables[8][256];
static INT32 amplitude_scale_tables[8][256];

INLINE UINT32 value_to_fixed(const UINT32 bits, const float value)
{
	const float float_shift = (float)(1 << bits);
	return (UINT32)(float_shift * value);
}

static void lfo_init(void)
{
	INT32 i;
	INT32 table;

	for (i = 0; i < 256; ++i)
	{
		if (i < 64)
			pitch_table[i] = i * 2 + 128;
		else if (i < 128)
			pitch_table[i] = 383 - i * 2;
		else if (i < 192)
			pitch_table[i] = 384 - i * 2;
		else
			pitch_table[i] = i * 2 - 383;

		if (i < 128)
			amplitude_table[i] = 255 - (i * 2);
		else
			amplitude_table[i] = (i * 2) - 256;
	}

	for (table = 0; table < 8; ++table)
	{
		float limit = PHASE_SCALE_LIMIT[table];
		for(i = -128; i < 128; ++i)
		{
			const float value = (limit * (float)i) / 128.0f;
			const float converted = powf(2.0f, value / 1200.0f);
			pitch_scale_tables[table][i + 128] = value_to_fixed(LFO_SHIFT, converted);
		}

		limit = -AMPLITUDE_SCALE_LIMIT[table];
		for(i = 0; i < 256; ++i)
		{
			const float value = (limit * (float)i) / 256.0f;
			const float converted = powf(10.0f, value / 20.0f);
			amplitude_scale_tables[table][i] = value_to_fixed(LFO_SHIFT, converted);
		}
	}
}

INLINE INT32 pitch_lfo_step(lfo_t *lfo)
{
	INT32 p;
	lfo->phase += lfo->phase_step;
	p = lfo->table[(lfo->phase >> LFO_SHIFT) & 0xff];
	p = lfo->scale[p];
	return p << (TL_SHIFT - LFO_SHIFT);
}

INLINE INT32 amplitude_lfo_step(lfo_t *lfo)
{
	INT32 p;
	lfo->phase += lfo->phase_step;
	p = lfo->table[(lfo->phase >> LFO_SHIFT) & 0xff];
	p = lfo->scale[p];
	return p << (TL_SHIFT - LFO_SHIFT);
}

INLINE void lfo_compute_step(MultiPCM *ptChip, lfo_t *lfo, UINT8 lfo_frequency, UINT8 lfo_scale, UINT8 amplitude_lfo)
{
	float step = (float)LFO_FREQ[lfo_frequency] * 256.0f / (float)ptChip->rate;
	lfo->phase_step = (UINT32)((float)(1 << LFO_SHIFT) * step);
	if (amplitude_lfo)
	{
		lfo->table = amplitude_table;
		lfo->scale = amplitude_scale_tables[lfo_scale];
	}
	else
	{
		lfo->table = pitch_table;
		lfo->scale = pitch_scale_tables[lfo_scale];
	}
}

static void write_slot(MultiPCM *ptChip, slot_t *slot, INT32 reg, UINT8 data)
{
	slot->regs[reg] = data;

	switch(reg)
	{
		case 0: // PANPOT
			slot->pan = (data >> 4) & 0xf;
			break;
		case 1: // Sample
			//according to YMF278 sample write causes some base params written to the regs (envelope+lfos)
			//the game should never change the sample while playing.
			// patched to load all sample data here, so registers 6 and 7 aren't overridden by KeyOn -Valley Bell
			init_sample(ptChip, &slot->sample, slot->regs[1] | ((slot->regs[2] & 1) << 8));
			write_slot(ptChip, slot, 6, slot->sample.lfo_vibrato_reg);
			write_slot(ptChip, slot, 7, slot->sample.lfo_amplitude_reg);
			break;
		case 2: //Pitch
		case 3:
			{
				UINT8 oct = ((slot->regs[3] >> 4) - 1) & 0xf;
				UINT32 pitch = ((slot->regs[3] & 0xf) << 6) | (slot->regs[2] >> 2);
				pitch = ptChip->freq_step_table[pitch];
				if (oct & 0x8)
					pitch >>= (16 - oct);
				else
					pitch <<= oct;
				slot->step = pitch / ptChip->rate;
			}
			break;
		case 4:     //KeyOn/Off (and more?)
			if (data & 0x80)       //KeyOn
			{
				slot->playing = 1;
				slot->base = slot->sample.start;
				slot->offset = 0;
				slot->prev_sample = 0;
				slot->total_level = slot->dest_total_level << TL_SHIFT;
				slot->format = slot->sample.format;

				envelope_generator_calc(ptChip, slot);
				slot->envelope_gen.state = ATTACK;
				slot->envelope_gen.volume = 0;

				if (ptChip->sega_banking)
				{
					slot->base &= 0x1fffff;
					if (slot->base & 0x100000)
					{
						if (slot->base & 0x080000)
							slot->base = (slot->base & 0x07ffff) | ptChip->bank1;
						else
							slot->base = (slot->base & 0x07ffff) | ptChip->bank0;
					}
				}

			}
			else
			{
				if (slot->playing)
				{
					if (slot->sample.release_reg != 0xf)
						slot->envelope_gen.state = RELEASE;
					else
						slot->playing = 0;
				}
			}
			break;
		case 5: // TL + Interpolation
			slot->dest_total_level = (data >> 1) & 0x7f;
			if (!(data & 1))   //Interpolate TL
			{
				if ((slot->total_level >> TL_SHIFT) > slot->dest_total_level)
					slot->total_level_step = ptChip->total_level_steps[0]; // decrease
				else
					slot->total_level_step = ptChip->total_level_steps[1]; // increase
			}
			else
			{
				slot->total_level = slot->dest_total_level << TL_SHIFT;
			}
			break;
		case 6: // LFO frequency + Pitch LFO
			if (data)
			{
				lfo_compute_step(ptChip, &slot->pitch_lfo, (slot->regs[6] >> 3) & 7, slot->regs[6] & 7, 0);
				lfo_compute_step(ptChip, &slot->amplitude_lfo, (slot->regs[6] >> 3) & 7, slot->regs[7] & 7, 1);
			}
			break;
		case 7: // Amplitude LFO
			if (data)
			{
				lfo_compute_step(ptChip, &slot->pitch_lfo, (slot->regs[6] >> 3) & 7, slot->regs[6] & 7, 0);
				lfo_compute_step(ptChip, &slot->amplitude_lfo, (slot->regs[6] >> 3) & 7, slot->regs[7] & 7, 1);
			}
			break;
	}
}

INLINE UINT8 read_byte(MultiPCM *ptChip, UINT32 addr)
{
	return ptChip->ROM[addr & ptChip->ROMMask];
}

static void MultiPCM_update(void *info, UINT32 samples, DEV_SMPL **outputs)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	UINT32 i, sl;

	if (ptChip->ROM == NULL)
	{
		memset(outputs[0], 0, samples * sizeof(DEV_SMPL));
		memset(outputs[1], 0, samples * sizeof(DEV_SMPL));
		return;
	}

	for (i = 0; i < samples; ++i)
	{
		DEV_SMPL smpl = 0;
		DEV_SMPL smpr = 0;
		for (sl = 0; sl < 28; ++sl)
		{
			slot_t *slot = ptChip->slots + sl;
			if (slot->playing && ! slot->muted)
			{
				UINT32 vol = (slot->total_level >> TL_SHIFT) | (slot->pan << 7);
				UINT32 spos = slot->offset >> TL_SHIFT;
				UINT32 step = slot->step;
				INT32 csample;
				INT32 fpart = slot->offset & ((1 << TL_SHIFT) - 1);
				INT32 sample;

				if (slot->format & 8)	// 12-bit linear
				{
					UINT32 adr = slot->base + (spos >> 2) * 6;
					switch (spos & 3)
					{
						case 0:
						{ // ab.c .... ....
							INT16 w0 = read_byte(ptChip, adr) << 8 | ((read_byte(ptChip, adr + 1) & 0xf) << 4);
							csample = w0;
							break;
						}
						case 1:
						{ // ..C. AB.. ....
							INT16 w0 = (read_byte(ptChip, adr + 2) << 8) | (read_byte(ptChip, adr + 1) & 0xf0);
							csample = w0;
							break;
						}
						case 2:
						{ // .... ..ab .c..
							INT16 w0 = read_byte(ptChip, adr + 3) << 8 | ((read_byte(ptChip, adr + 4) & 0xf) << 4);
							csample = w0;
							break;
						}
						case 3:
						{ // .... .... C.AB
							INT16 w0 = (read_byte(ptChip, adr + 5) << 8) | (read_byte(ptChip, adr + 4) & 0xf0);
							csample = w0;
							break;
						}
					}
				}
				else
				{
					csample = (INT16)(read_byte(ptChip, slot->base + spos) << 8);
				}

				sample = (csample * fpart + slot->prev_sample * ((1 << TL_SHIFT) - fpart)) >> TL_SHIFT;

				if (slot->regs[6] & 7) // Vibrato enabled
				{
					step = step * pitch_lfo_step(&slot->pitch_lfo);
					step >>= TL_SHIFT;
				}

				slot->offset += step;
				if (slot->offset >= (slot->sample.end << TL_SHIFT))
				{
					slot->offset = slot->sample.loop << TL_SHIFT;
				}

				if (spos ^ (slot->offset >> TL_SHIFT))
				{
					slot->prev_sample = csample;
				}

				if ((slot->total_level >> TL_SHIFT) != slot->dest_total_level)
				{
					slot->total_level += slot->total_level_step;
				}

				if (slot->regs[7] & 7) // Tremolo enabled
				{
					sample = sample * amplitude_lfo_step(&slot->amplitude_lfo);
					sample >>= TL_SHIFT;
				}

				sample = (sample * envelope_generator_update(slot)) >> 10;

				smpl += (left_pan_table[vol] * sample) >> TL_SHIFT;
				smpr += (right_pan_table[vol] * sample) >> TL_SHIFT;
			}
		}

		outputs[0][i] = smpl;
		outputs[1][i] = smpr;
	}
}

static UINT8 multipcm_r(void *info, UINT8 offset)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	return 0;
}

static UINT8 device_start_multipcm(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	MultiPCM *ptChip;
	INT32 i;

	ptChip = (MultiPCM *)calloc(1, sizeof(MultiPCM));
	if (ptChip == NULL)
		return 0xFF;
	
	ptChip->ROM = NULL;
	ptChip->ROMSize = 0x00;
	ptChip->ROMMask = 0x00;
	ptChip->rate = (float)cfg->clock / MULTIPCM_CLOCKDIV;

	if (! IsInit)
	{
		INT32 level;

		IsInit = 1;

		// Volume + pan table
		for (level = 0; level < 0x80; ++level)
		{
			const float vol_db = (float)level * (-24.0f) / 64.0f;
			const float total_level = powf(10.0f, vol_db / 20.0f) / 4.0f;
			INT32 pan;

			for (pan = 0; pan < 0x10; ++pan)
			{
				float pan_left, pan_right;
				if (pan == 0x8)
				{
					pan_left = 0.0;
					pan_right = 0.0;
				}
				else if (pan == 0x0)
				{
					pan_left = 1.0;
					pan_right = 1.0;
				}
				else if (pan & 0x8)
				{
					const INT32 inverted_pan = 0x10 - pan;
					const float pan_vol_db = (float)inverted_pan * (-12.0f) / 4.0f;

					pan_left = 1.0;
					pan_right = powf(10.0f, pan_vol_db / 20.0f);

					if ((inverted_pan & 0x7) == 7)
						pan_right = 0.0;
				}
				else
				{
					const float pan_vol_db = (float)pan * (-12.0f) / 4.0f;

					pan_left = powf(10.0f, pan_vol_db / 20.0f);
					pan_right = 1.0;

					if ((pan & 0x7) == 7)
						pan_left = 0.0;
				}

				left_pan_table[(pan << 7) | level] = value_to_fixed(TL_SHIFT, pan_left * total_level);
				right_pan_table[(pan << 7) | level] = value_to_fixed(TL_SHIFT, pan_right * total_level);
			}
		}

		// build the linear->exponential ramps
		for(i = 0; i < 0x400; ++i)
		{
			const float db = -(96.0f - (96.0f * (float)i / (float)0x400));
			const float exp_volume = powf(10.0f, db / 20.0f);
			linear_to_exp_volume[i] = value_to_fixed(TL_SHIFT, exp_volume);
		}

		lfo_init();
	}

	//Pitch steps
	for (i = 0; i < 0x400; ++i)
	{
		const float fcent = ptChip->rate * (1024.0f + (float)i) / 1024.0f;
		ptChip->freq_step_table[i] = value_to_fixed(TL_SHIFT, fcent);
	}

	// Envelope steps
	for (i = 4; i < 0x40; ++i)
	{
		// Times are based on 44100Hz clock, adjust to real chip clock
		ptChip->attack_step[i] = (float)(0x400 << EG_SHIFT) / (float)(BASE_TIMES[i] * 44100.0 / 1000.0);
		ptChip->decay_release_step[i] = (float)(0x400 << EG_SHIFT) / (float)(BASE_TIMES[i] * attack_rate_to_decay_rate * 44100.0 / 1000.0);
	}
	ptChip->attack_step[0] = ptChip->attack_step[1] = ptChip->attack_step[2] = ptChip->attack_step[3] = 0;
	ptChip->attack_step[0x3f] = 0x400 << EG_SHIFT;
	ptChip->decay_release_step[0] = ptChip->decay_release_step[1] = ptChip->decay_release_step[2] = ptChip->decay_release_step[3] = 0;

	// Total level interpolation steps
	ptChip->total_level_steps[0] = -(float)(0x80 << TL_SHIFT) / (78.2f * 44100.0f / 1000.0f); // lower
	ptChip->total_level_steps[1] = (float)(0x80 << TL_SHIFT) / (78.2f * 2 * 44100.0f / 1000.0f); // raise

	ptChip->sega_banking = 0;
	ptChip->bank0 = ptChip->bank1 = 0x000000;

	multipcm_set_mute_mask(ptChip, 0x00);

	ptChip->_devData.chipInf = ptChip;
	INIT_DEVINF(retDevInf, &ptChip->_devData, (UINT32)ptChip->rate, &devDef);

	return 0x00;
}


static void device_stop_multipcm(void *info)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	
	free(ptChip->ROM);
	free(ptChip);
	
	return;
}

static void device_reset_multipcm(void *info)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	INT32 slot;
	
	for (slot = 0; slot < 28; ++slot)
	{
		ptChip->slots[slot].slot_index = slot;
		ptChip->slots[slot].playing = 0;
	}
	
	return;
}


static void multipcm_write(void *info, UINT8 offset, UINT8 data)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	switch(offset)
	{
		case 0:     //Data write
			if (ptChip->cur_slot == -1)
				return;
			write_slot(ptChip, ptChip->slots + ptChip->cur_slot, ptChip->address, data);
			break;
		case 1:
			ptChip->cur_slot = VALUE_TO_CHANNEL[data & 0x1f];
			break;

		case 2:
			ptChip->address = (data > 7) ? 7 : data;
			break;

		// special SEGA banking
		case 0x10:	// 1 MB banking (Sega Model 1)
			ptChip->sega_banking = 1;
			ptChip->bank0 = (data << 20) | 0x000000;
			ptChip->bank1 = (data << 20) | 0x080000;
			break;
		case 0x11:	// 512 KB banking - low bank (Sega Multi 32)
			ptChip->sega_banking = 1;
			ptChip->bank0 = data << 19;
			break;
		case 0x12:	// 512 KB banking - high bank (Sega Multi 32)
			ptChip->sega_banking = 1;
			ptChip->bank1 = data << 19;
			break;
	}
}

static void multipcm_w_quick(void *info, UINT8 offset, UINT8 data)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	ptChip->cur_slot = VALUE_TO_CHANNEL[(offset >> 3) & 0x1F];
	ptChip->address = offset & 0x07;
	if (ptChip->cur_slot == -1)
		return;
	write_slot(ptChip, ptChip->slots + ptChip->cur_slot, ptChip->address, data);
}

/* MAME/M1 access functions */

static void multipcm_alloc_rom(void* info, UINT32 memsize)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	
	if (ptChip->ROMSize == memsize)
		return;
	
	ptChip->ROM = (UINT8*)realloc(ptChip->ROM, memsize);
	ptChip->ROMSize = memsize;
	memset(ptChip->ROM, 0xFF, memsize);
	
	ptChip->ROMMask = pow2_mask(memsize);
	
	return;
}

static void multipcm_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	MultiPCM *ptChip = (MultiPCM *)info;
	
	if (offset > ptChip->ROMSize)
		return;
	if (offset + length > ptChip->ROMSize)
		length = ptChip->ROMSize - offset;
	
	memcpy(ptChip->ROM + offset, data, length);
	
	return;
}


static void multipcm_set_mute_mask(void *info, UINT32 MuteMask)
{
	MultiPCM* ptChip = (MultiPCM *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 28; CurChn ++)
		ptChip->slots[CurChn].muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
