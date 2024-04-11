// license:BSD-3-Clause
// copyright-holders:Wilbert Pol, Anthony Kruize
// thanks-to:Shay Green
/**************************************************************************************
* Game Boy sound emulation (c) Anthony Kruize (trandor@labyrinth.net.au)
*
* Anyways, sound on the Game Boy consists of 4 separate 'channels'
*   Sound1 = Quadrangular waves with SWEEP and ENVELOPE functions  (NR10,11,12,13,14)
*   Sound2 = Quadrangular waves with ENVELOPE functions (NR21,22,23,24)
*   Sound3 = Wave patterns from WaveRAM (NR30,31,32,33,34)
*   Sound4 = White noise with an envelope (NR41,42,43,44)
*
* Each sound channel has 2 modes, namely ON and OFF...  whoa
*
* These tend to be the two most important equations in
* converting between Hertz and GB frequency registers:
* (Sounds will have a 2.4% higher frequency on Super GB.)
*       gb = 2048 - (131072 / Hz)
*       Hz = 131072 / (2048 - gb)
*
* Changes:
*
*   10/2/2002       AK - Preliminary sound code.
*   13/2/2002       AK - Added a hack for mode 4, other fixes.
*   23/2/2002       AK - Use lookup tables, added sweep to mode 1. Re-wrote the square
*                        wave generation.
*   13/3/2002       AK - Added mode 3, better lookup tables, other adjustments.
*   15/3/2002       AK - Mode 4 can now change frequencies.
*   31/3/2002       AK - Accidently forgot to handle counter/consecutive for mode 1.
*    3/4/2002       AK - Mode 1 sweep can still occur if shift is 0.  Don't let frequency
*                        go past the maximum allowed value. Fixed Mode 3 length table.
*                        Slight adjustment to Mode 4's period table generation.
*    5/4/2002       AK - Mode 4 is done correctly, using a polynomial counter instead
*                        of being a total hack.
*    6/4/2002       AK - Slight tweak to mode 3's frequency calculation.
*   13/4/2002       AK - Reset envelope value when sound is initialized.
*   21/4/2002       AK - Backed out the mode 3 frequency calculation change.
*                        Merged init functions into gameboy_sound_w().
*   14/5/2002       AK - Removed magic numbers in the fixed point math.
*   12/6/2002       AK - Merged SOUNDx structs into one SOUND struct.
*  26/10/2002       AK - Finally fixed channel 3!
* xx/4-5/2016       WP - Rewrote sound core. Most of the code is not optimized yet.

TODO:
- Implement different behavior of CGB-02.
- Implement different behavior of CGB-05.
- Perform more tests on real hardware to figure out when the frequency counters are
  reloaded.
- Perform more tests on real hardware to understand when changes to the noise divisor
  and shift kick in.
- Optimize the channel update methods.

***************************************************************************************/

#include <stdlib.h>	// for rand
#include <string.h>	// for memset

#include "../../stdtype.h"
#include "../../stdbool.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../RatioCntr.h"
#include "gb.h"


static UINT8 gb_wave_r(void *chip, UINT8 offset);
static void gb_wave_w(void *chip, UINT8 offset, UINT8 data);
static UINT8 gb_sound_r(void *chip, UINT8 offset);
static void gb_sound_w(void *chip, UINT8 offset, UINT8 data);

static void gameboy_update(void *chip, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_gameboy_sound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_gameboy_sound(void *chip);
static void device_reset_gameboy_sound(void *chip);

static void gameboy_sound_set_mute_mask(void *chip, UINT32 MuteMask);
static UINT32 gameboy_sound_get_mute_mask(void *chip);
static void gameboy_sound_set_options(void *chip, UINT32 Flags);



static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, gb_sound_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, gb_sound_r},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, gameboy_sound_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"GameBoy DMG", "MAME", FCC_MAME,
	
	device_start_gameboy_sound,
	device_stop_gameboy_sound,
	device_reset_gameboy_sound,
	gameboy_update,
	
	gameboy_sound_set_options,	// SetOptionBits
	gameboy_sound_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_GB_DMG[] =
{
	&devDef,
	NULL
};


/***************************************************************************
    CONSTANTS
***************************************************************************/

#define NR10 0x00
#define NR11 0x01
#define NR12 0x02
#define NR13 0x03
#define NR14 0x04
// 0x05
#define NR21 0x06
#define NR22 0x07
#define NR23 0x08
#define NR24 0x09
#define NR30 0x0A
#define NR31 0x0B
#define NR32 0x0C
#define NR33 0x0D
#define NR34 0x0E
// 0x0F
#define NR41 0x10
#define NR42 0x11
#define NR43 0x12
#define NR44 0x13
#define NR50 0x14
#define NR51 0x15
#define NR52 0x16
// 0x17 - 0x1F
#define AUD3W0 0x20
#define AUD3W1 0x21
#define AUD3W2 0x22
#define AUD3W3 0x23
#define AUD3W4 0x24
#define AUD3W5 0x25
#define AUD3W6 0x26
#define AUD3W7 0x27
#define AUD3W8 0x28
#define AUD3W9 0x29
#define AUD3WA 0x2A
#define AUD3WB 0x2B
#define AUD3WC 0x2C
#define AUD3WD 0x2D
#define AUD3WE 0x2E
#define AUD3WF 0x2F

#define FRAME_CYCLES 8192

/* Represents wave duties of 12.5%, 25%, 50% and 75% */
static const int wave_duty_table[4][8] =
{
	{ -1, -1, -1, -1, -1, -1, -1,  1},
	{  1, -1, -1, -1, -1, -1, -1,  1},
	{  1, -1, -1, -1, -1,  1,  1,  1},
	{ -1,  1,  1,  1,  1,  1,  1, -1}
};


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

struct SOUND
{
	/* Common */
	UINT8  reg[5];
	bool   on;
	UINT8  channel;
	UINT8  length;
	UINT8  length_mask;
	bool   length_counting;
	bool   length_enabled;
	/* Mode 1, 2, 3 */
	INT32  cycles_left;
	INT8   duty;
	/* Mode 1, 2, 4 */
	bool   envelope_enabled;
	INT8   envelope_value;
	INT8   envelope_direction;
	UINT8  envelope_time;
	UINT8  envelope_count;
	INT8   signal;
	/* Mode 1 */
	UINT16 frequency;
	UINT16 frequency_counter;
	bool   sweep_enabled;
	bool   sweep_neg_mode_used;
	UINT8  sweep_shift;
	INT32  sweep_direction;
	UINT8  sweep_time;
	UINT8  sweep_count;
	/* Mode 3 */
	UINT8  level;
	UINT8  offset;
	UINT32 duty_count;
	INT8   current_sample;
	bool   sample_reading;
	/* Mode 4 */
	bool   noise_short;
	UINT16 noise_lfsr;
	UINT8  Muted;
};

struct SOUNDC
{
	UINT8 on;
	UINT8 vol_left;
	UINT8 vol_right;
	UINT8 mode1_left;
	UINT8 mode1_right;
	UINT8 mode2_left;
	UINT8 mode2_right;
	UINT8 mode3_left;
	UINT8 mode3_right;
	UINT8 mode4_left;
	UINT8 mode4_right;
	UINT32 cycles;
	bool wave_ram_locked;
};


#define GBMODE_DMG      0x00
#define GBMODE_CGB04    0x01
typedef struct _gb_sound_t gb_sound_t;
struct _gb_sound_t
{
	DEV_DATA _devData;

	UINT32 rate;

	struct SOUND  snd_1;
	struct SOUND  snd_2;
	struct SOUND  snd_3;
	struct SOUND  snd_4;
	struct SOUNDC snd_control;

	UINT8 snd_regs[0x30];

	RATIO_CNTR cycleCntr;

	UINT8 gbMode;
	UINT8 BoostWaveChn;
	UINT8 NoWaveCorrupt;
	UINT8 LegacyMode;
};


static void gb_corrupt_wave_ram(gb_sound_t *gb);
static void gb_apu_power_off(gb_sound_t *gb);
static void gb_tick_length(struct SOUND *snd);
static INT32 gb_calculate_next_sweep(struct SOUND *snd);
INLINE bool gb_dac_enabled(struct SOUND *snd);
INLINE UINT32 gb_noise_period_cycles(gb_sound_t *gb);

/***************************************************************************
    IMPLEMENTATION
***************************************************************************/

static UINT8 gb_wave_r(void *chip, UINT8 offset)
{
	gb_sound_t *gb = (gb_sound_t *)chip;

	//gb_update_state(gb, 0);

	if (gb->snd_3.on)
	{
		if (gb->gbMode == GBMODE_DMG)
			return gb->snd_3.sample_reading ? gb->snd_regs[AUD3W0 + (gb->snd_3.offset/2)] : 0xFF;
		else if (gb->gbMode == GBMODE_CGB04)
			return gb->snd_regs[AUD3W0 + (gb->snd_3.offset/2)];
	}

	return gb->snd_regs[AUD3W0 + offset];
}

static void gb_wave_w(void *chip, UINT8 offset, UINT8 data)
{
	gb_sound_t *gb = (gb_sound_t *)chip;

	//gb_update_state(gb, 0);

	if (gb->snd_3.on)
	{
		if (gb->gbMode == GBMODE_DMG)
		{
			if (gb->snd_3.sample_reading)
			{
				gb->snd_regs[AUD3W0 + (gb->snd_3.offset/2)] = data;
			}
		}
		else if (gb->gbMode == GBMODE_CGB04)
		{
			gb->snd_regs[AUD3W0 + (gb->snd_3.offset/2)] = data;
		}
	}
	else
	{
		gb->snd_regs[AUD3W0 + offset] = data;
	}
}

static UINT8 gb_sound_r(void *chip, UINT8 offset)
{
	static const UINT8 read_mask[0x40] =
	{
		0x80,0x3F,0x00,0xFF,0xBF,0xFF,0x3F,0x00,0xFF,0xBF,0x7F,0xFF,0x9F,0xFF,0xBF,0xFF,
		0xFF,0x00,0x00,0xBF,0x00,0x00,0x70,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	};
	gb_sound_t *gb = (gb_sound_t *)chip;

	//gb_update_state(gb, 0);

	if (offset < AUD3W0)
	{
		if (gb->snd_control.on)
		{
			if (offset == NR52)
			{
				return (gb->snd_regs[NR52]&0xf0) | (gb->snd_1.on ? 1 : 0) | (gb->snd_2.on ? 2 : 0) | (gb->snd_3.on ? 4 : 0) | (gb->snd_4.on ? 8 : 0) | 0x70;
			}
			return gb->snd_regs[offset] | read_mask[offset & 0x3F];
		}
		else
		{
			return read_mask[offset & 0x3F];
		}
	}
	else if (offset <= AUD3WF)
	{
		return gb_wave_r(gb, offset - AUD3W0);
	}
	return 0xFF;
}

static void gb_sound_w_internal(gb_sound_t *gb, UINT8 offset, UINT8 data)
{
	/* Store the value */
	UINT8 old_data = gb->snd_regs[offset];

	if (gb->snd_control.on)
	{
		gb->snd_regs[offset] = data;
	}

	switch (offset)
	{
	/*MODE 1 */
	case NR10: /* Sweep (R/W) */
		gb->snd_1.reg[0] = data;
		gb->snd_1.sweep_shift = data & 0x7;
		gb->snd_1.sweep_direction = (data & 0x8) ? -1 : 1;
		gb->snd_1.sweep_time = (data & 0x70) >> 4;
		if ((old_data & 0x08) && !(data & 0x08) && gb->snd_1.sweep_neg_mode_used)
		{
			gb->snd_1.on = false;
		}
		break;
	case NR11: /* Sound length/Wave pattern duty (R/W) */
		gb->snd_1.reg[1] = data;
		if (gb->snd_control.on)
		{
			gb->snd_1.duty = (data & 0xc0) >> 6;
		}
		gb->snd_1.length = data & 0x3f;
		gb->snd_1.length_counting = true;
		break;
	case NR12: /* Envelope (R/W) */
		gb->snd_1.reg[2] = data;
		gb->snd_1.envelope_value = data >> 4;
		gb->snd_1.envelope_direction = (data & 0x8) ? 1 : -1;
		gb->snd_1.envelope_time = data & 0x07;
		if (!gb_dac_enabled(&gb->snd_1))
		{
			gb->snd_1.on = false;
		}
		break;
	case NR13: /* Frequency lo (R/W) */
		gb->snd_1.reg[3] = data;
		// Only enabling the frequency line breaks blarggs's sound test #5
		// This condition may not be correct
		if (!gb->snd_1.sweep_enabled)
		{
			gb->snd_1.frequency = ((gb->snd_1.reg[4] & 0x7) << 8) | gb->snd_1.reg[3];
		}
		break;
	case NR14: /* Frequency hi / Initialize (R/W) */
		gb->snd_1.reg[4] = data;
		{
			bool length_was_enabled = gb->snd_1.length_enabled;

			gb->snd_1.length_enabled = (data & 0x40) ? true : false;
			gb->snd_1.frequency = ((gb->snd_regs[NR14] & 0x7) << 8) | gb->snd_1.reg[3];

			if (!length_was_enabled && !(gb->snd_control.cycles & FRAME_CYCLES) && gb->snd_1.length_counting)
			{
				if (gb->snd_1.length_enabled)
				{
					gb_tick_length(&gb->snd_1);
				}
			}

			if (data & 0x80)
			{
				gb->snd_1.on = true;
				gb->snd_1.envelope_enabled = true;
				gb->snd_1.envelope_value = gb->snd_1.reg[2] >> 4;
				gb->snd_1.envelope_count = gb->snd_1.envelope_time;
				gb->snd_1.sweep_count = gb->snd_1.sweep_time;
				gb->snd_1.sweep_neg_mode_used = false;
				gb->snd_1.signal = 0;
				if (gb->LegacyMode)
					gb->snd_1.length = gb->snd_1.reg[1] & 0x3f;	// VGM log fix -Valley Bell
				gb->snd_1.length_counting = true;
				gb->snd_1.frequency = ((gb->snd_1.reg[4] & 0x7) << 8) | gb->snd_1.reg[3];
				gb->snd_1.frequency_counter = gb->snd_1.frequency;
				gb->snd_1.cycles_left = 0;
				gb->snd_1.duty_count = 0;
				gb->snd_1.sweep_enabled = (gb->snd_1.sweep_shift != 0) || (gb->snd_1.sweep_time != 0);
				if (!gb_dac_enabled(&gb->snd_1))
				{
					gb->snd_1.on = false;
				}
				if (gb->snd_1.sweep_shift > 0)
				{
					gb_calculate_next_sweep(&gb->snd_1);
				}

				if (gb->snd_1.length == 0 && gb->snd_1.length_enabled && !(gb->snd_control.cycles & FRAME_CYCLES))
				{
					gb_tick_length(&gb->snd_1);
				}
			}
			else
			{
				// This condition may not be correct
				if (!gb->snd_1.sweep_enabled)
				{
					gb->snd_1.frequency = ((gb->snd_1.reg[4] & 0x7) << 8) | gb->snd_1.reg[3];
				}
			}
		}
		break;

	/*MODE 2 */
	case NR21: /* Sound length/Wave pattern duty (R/W) */
		gb->snd_2.reg[1] = data;
		if (gb->snd_control.on)
		{
			gb->snd_2.duty = (data & 0xc0) >> 6;
		}
		gb->snd_2.length = data & 0x3f;
		gb->snd_2.length_counting = true;
		break;
	case NR22: /* Envelope (R/W) */
		gb->snd_2.reg[2] = data;
		gb->snd_2.envelope_value = data >> 4;
		gb->snd_2.envelope_direction = (data & 0x8) ? 1 : -1;
		gb->snd_2.envelope_time = data & 0x07;
		if (!gb_dac_enabled(&gb->snd_2))
		{
			gb->snd_2.on = false;
		}
		break;
	case NR23: /* Frequency lo (R/W) */
		gb->snd_2.reg[3] = data;
		gb->snd_2.frequency = ((gb->snd_2.reg[4] & 0x7) << 8) | gb->snd_2.reg[3];
		break;
	case NR24: /* Frequency hi / Initialize (R/W) */
		gb->snd_2.reg[4] = data;
		{
			bool length_was_enabled = gb->snd_2.length_enabled;

			gb->snd_2.length_enabled = (data & 0x40) ? true : false;

			if (!length_was_enabled && !(gb->snd_control.cycles & FRAME_CYCLES) && gb->snd_2.length_counting)
			{
				if (gb->snd_2.length_enabled)
				{
					gb_tick_length(&gb->snd_2);
				}
			}

			if (data & 0x80)
			{
				gb->snd_2.on = true;
				gb->snd_2.envelope_enabled = true;
				gb->snd_2.envelope_value = gb->snd_2.reg[2] >> 4;
				gb->snd_2.envelope_count = gb->snd_2.envelope_time;
				gb->snd_2.frequency = ((gb->snd_2.reg[4] & 0x7) << 8) | gb->snd_2.reg[3];
				gb->snd_2.frequency_counter = gb->snd_2.frequency;
				gb->snd_2.cycles_left = 0;
				gb->snd_2.duty_count = 0;
				gb->snd_2.signal = 0;
				if (gb->LegacyMode)
					gb->snd_2.length = gb->snd_2.reg[1] & 0x3f;	// VGM log fix -Valley Bell
				gb->snd_2.length_counting = true;

				if (!gb_dac_enabled(&gb->snd_2))
				{
					gb->snd_2.on = false;
				}

				if (gb->snd_2.length == 0 && gb->snd_2.length_enabled && !(gb->snd_control.cycles & FRAME_CYCLES))
				{
					gb_tick_length(&gb->snd_2);
				}
			}
			else
			{
				gb->snd_2.frequency = ((gb->snd_2.reg[4] & 0x7) << 8) | gb->snd_2.reg[3];
			}
		}
		break;

	/*MODE 3 */
	case NR30: /* Sound On/Off (R/W) */
		gb->snd_3.reg[0] = data;
		if (!gb_dac_enabled(&gb->snd_3))
		{
			gb->snd_3.on = false;
		}
		else if (gb->LegacyMode)
			gb->snd_3.on = true;	// even more VGM log fix -Valley Bell
		break;
	case NR31: /* Sound Length (R/W) */
		gb->snd_3.reg[1] = data;
		gb->snd_3.length = data;
		gb->snd_3.length_counting = true;
		break;
	case NR32: /* Select Output Level */
		gb->snd_3.reg[2] = data;
		gb->snd_3.level = (data & 0x60) >> 5;
		break;
	case NR33: /* Frequency lo (W) */
		gb->snd_3.reg[3] = data;
		gb->snd_3.frequency = ((gb->snd_3.reg[4] & 0x7) << 8) | gb->snd_3.reg[3];
		break;
	case NR34: /* Frequency hi / Initialize (W) */
		gb->snd_3.reg[4] = data;
		{
			bool length_was_enabled = gb->snd_3.length_enabled;

			gb->snd_3.length_enabled = (data & 0x40) ? true : false;

			if (!length_was_enabled && !(gb->snd_control.cycles & FRAME_CYCLES) && gb->snd_3.length_counting)
			{
				if (gb->snd_3.length_enabled)
				{
					gb_tick_length(&gb->snd_3);
				}
			}

			if (data & 0x80)
			{
				if (gb->snd_3.on && gb->snd_3.frequency_counter == 0x7ff)
				{
					gb_corrupt_wave_ram(gb);
				}
				gb->snd_3.on = true;
				gb->snd_3.offset = 0;
				gb->snd_3.duty = 1;
				gb->snd_3.duty_count = 0;
				if (gb->LegacyMode)
					gb->snd_3.length = gb->snd_3.reg[1];	// VGM log fix -Valley Bell
				gb->snd_3.length_counting = true;
				gb->snd_3.frequency = ((gb->snd_3.reg[4] & 0x7) << 8) | gb->snd_3.reg[3];
				gb->snd_3.frequency_counter = gb->snd_3.frequency;
				// There is a tiny bit of delay in starting up the wave channel
				gb->snd_3.cycles_left = -6;
				gb->snd_3.sample_reading = false;

				if (!gb_dac_enabled(&gb->snd_3))
				{
					gb->snd_3.on = false;
				}

				if (gb->snd_3.length == 0 && gb->snd_3.length_enabled && !(gb->snd_control.cycles & FRAME_CYCLES))
				{
					gb_tick_length(&gb->snd_3);
				}
			}
			else
			{
				gb->snd_3.frequency = ((gb->snd_3.reg[4] & 0x7) << 8) | gb->snd_3.reg[3];
			}
		}
		break;

	/*MODE 4 */
	case NR41: /* Sound Length (R/W) */
		gb->snd_4.reg[1] = data;
		gb->snd_4.length = data & 0x3f;
		gb->snd_4.length_counting = true;
		break;
	case NR42: /* Envelope (R/W) */
		gb->snd_4.reg[2] = data;
		gb->snd_4.envelope_value = data >> 4;
		gb->snd_4.envelope_direction = (data & 0x8) ? 1 : -1;
		gb->snd_4.envelope_time = data & 0x07;
		if (!gb_dac_enabled(&gb->snd_4))
		{
			gb->snd_4.on = false;
		}
		break;
	case NR43: /* Polynomial Counter/Frequency */
		gb->snd_4.reg[3] = data;
		gb->snd_4.noise_short = (data & 0x8);
		break;
	case NR44: /* Counter/Consecutive / Initialize (R/W)  */
		gb->snd_4.reg[4] = data;
		{
			bool length_was_enabled = gb->snd_4.length_enabled;

			gb->snd_4.length_enabled = (data & 0x40) ? true : false;

			if (!length_was_enabled && !(gb->snd_control.cycles & FRAME_CYCLES) && gb->snd_4.length_counting)
			{
				if (gb->snd_4.length_enabled)
				{
					gb_tick_length(&gb->snd_4);
				}
			}

			if (data & 0x80)
			{
				gb->snd_4.on = true;
				gb->snd_4.envelope_enabled = true;
				gb->snd_4.envelope_value = gb->snd_4.reg[2] >> 4;
				gb->snd_4.envelope_count = gb->snd_4.envelope_time;
				gb->snd_4.frequency_counter = 0;
				gb->snd_4.cycles_left = gb_noise_period_cycles(gb);
				gb->snd_4.signal = -1;
				gb->snd_4.noise_lfsr = 0x7fff;
				if (gb->LegacyMode)
					gb->snd_4.length = gb->snd_4.reg[1] & 0x3f;	// VGM log fix -Valley Bell
				gb->snd_4.length_counting = true;

				if (!gb_dac_enabled(&gb->snd_4))
				{
					gb->snd_4.on = false;
				}

				if (gb->snd_4.length == 0 && gb->snd_4.length_enabled && !(gb->snd_control.cycles & FRAME_CYCLES))
				{
					gb_tick_length(&gb->snd_4);
				}
			}
		}
		break;

	/* CONTROL */
	case NR50: /* Channel Control / On/Off / Volume (R/W)  */
		gb->snd_control.vol_left = data & 0x7;
		gb->snd_control.vol_right = (data & 0x70) >> 4;
		break;
	case NR51: /* Selection of Sound Output Terminal */
		gb->snd_control.mode1_right = data & 0x1;
		gb->snd_control.mode1_left = (data & 0x10) >> 4;
		gb->snd_control.mode2_right = (data & 0x2) >> 1;
		gb->snd_control.mode2_left = (data & 0x20) >> 5;
		gb->snd_control.mode3_right = (data & 0x4) >> 2;
		gb->snd_control.mode3_left = (data & 0x40) >> 6;
		gb->snd_control.mode4_right = (data & 0x8) >> 3;
		gb->snd_control.mode4_left = (data & 0x80) >> 7;
		break;
	case NR52: // Sound On/Off (R/W)
		// Only bit 7 is writable, writing to bits 0-3 does NOT enable or disable sound. They are read-only.
		if (!(data & 0x80))
		{
			// On DMG the length counters are not affected and not clocked
			// powering off should actually clear all registers
			gb_apu_power_off(gb);
		}
		else
		{
			if (!gb->snd_control.on)
			{
				// When switching on, the next step should be 0.
				gb->snd_control.cycles |= 7 * FRAME_CYCLES;
			}
		}
		gb->snd_control.on = (data & 0x80) ? true : false;
		gb->snd_regs[NR52] = data & 0x80;
		break;
	}
}

static void gb_sound_w(void *chip, UINT8 offset, UINT8 data)
{
	gb_sound_t *gb = (gb_sound_t *)chip;

	//gb_update_state(gb, 0);

	if (offset < AUD3W0)
	{
		if (gb->gbMode == GBMODE_DMG)
		{
			/* Only register NR52 is accessible if the sound controller is disabled */
			if( !gb->snd_control.on && offset != NR52 && offset != NR11 && offset != NR21 && offset != NR31 && offset != NR41)
				return;
		}
		else if (gb->gbMode == GBMODE_CGB04)
		{
			/* Only register NR52 is accessible if the sound controller is disabled */
			if (!gb->snd_control.on && offset != NR52)
				return;
		}

		gb_sound_w_internal(gb, offset, data);
	}
	else if (offset <= AUD3WF)
	{
		gb_wave_w(gb, offset - AUD3W0, data);
	}
}

static void gb_corrupt_wave_ram(gb_sound_t *gb)
{
	if (gb->gbMode != GBMODE_DMG || gb->NoWaveCorrupt)
		return;

	if (gb->snd_3.offset < 8)
	{
		gb->snd_regs[AUD3W0] = gb->snd_regs[AUD3W0 + (gb->snd_3.offset/2)];
	}
	else
	{
		int i;
		for (i = 0; i < 4; i++)
		{
			gb->snd_regs[AUD3W0 + i] = gb->snd_regs[AUD3W0 + ((gb->snd_3.offset / 2) & ~0x03) + i];
		}
	}
}


static void gb_apu_power_off(gb_sound_t *gb)
{
	int i;

	switch(gb->gbMode)
	{
	case GBMODE_DMG:
		gb_sound_w_internal(gb, NR10, 0x00);
		gb->snd_1.duty = 0;
		gb->snd_regs[NR11] = 0;
		gb_sound_w_internal(gb, NR12, 0x00);
		gb_sound_w_internal(gb, NR13, 0x00);
		gb_sound_w_internal(gb, NR14, 0x00);
		gb->snd_1.length_counting = false;
		gb->snd_1.sweep_neg_mode_used = false;

		gb->snd_regs[NR21] = 0;
		gb_sound_w_internal(gb, NR22, 0x00);
		gb_sound_w_internal(gb, NR23, 0x00);
		gb_sound_w_internal(gb, NR24, 0x00);
		gb->snd_2.length_counting = false;

		gb_sound_w_internal(gb, NR30, 0x00);
		gb_sound_w_internal(gb, NR32, 0x00);
		gb_sound_w_internal(gb, NR33, 0x00);
		gb_sound_w_internal(gb, NR34, 0x00);
		gb->snd_3.length_counting = false;
		gb->snd_3.current_sample = 0;

		gb->snd_regs[NR41] = 0;
		gb_sound_w_internal(gb, NR42, 0x00);
		gb_sound_w_internal(gb, NR43, 0x00);
		gb_sound_w_internal(gb, NR44, 0x00);
		gb->snd_4.length_counting = false;
		gb->snd_4.cycles_left = gb_noise_period_cycles(gb);
		break;
	case GBMODE_CGB04:
		gb_sound_w_internal(gb, NR10, 0x00);
		gb->snd_1.duty = 0;
		gb_sound_w_internal(gb, NR11, 0x00);
		gb_sound_w_internal(gb, NR12, 0x00);
		gb_sound_w_internal(gb, NR13, 0x00);
		gb_sound_w_internal(gb, NR14, 0x00);
		gb->snd_1.length_counting = false;
		gb->snd_1.sweep_neg_mode_used = false;

		gb_sound_w_internal(gb, NR21, 0x00);
		gb_sound_w_internal(gb, NR22, 0x00);
		gb_sound_w_internal(gb, NR23, 0x00);
		gb_sound_w_internal(gb, NR24, 0x00);
		gb->snd_2.length_counting = false;

		gb_sound_w_internal(gb, NR30, 0x00);
		gb_sound_w_internal(gb, NR31, 0x00);
		gb_sound_w_internal(gb, NR32, 0x00);
		gb_sound_w_internal(gb, NR33, 0x00);
		gb_sound_w_internal(gb, NR34, 0x00);
		gb->snd_3.length_counting = false;
		gb->snd_3.current_sample = 0;

		gb_sound_w_internal(gb, NR41, 0x00);
		gb_sound_w_internal(gb, NR42, 0x00);
		gb_sound_w_internal(gb, NR43, 0x00);
		gb_sound_w_internal(gb, NR44, 0x00);
		gb->snd_4.length_counting = false;
		gb->snd_4.cycles_left = gb_noise_period_cycles(gb);
		break;
	}

	gb->snd_1.on = false;
	gb->snd_2.on = false;
	gb->snd_3.on = false;
	gb->snd_4.on = false;

	gb->snd_control.wave_ram_locked = false;

	for (i = NR44 + 1; i < NR52; i++)
	{
		gb_sound_w_internal(gb, i, 0x00);
	}

	return;
}


static void gb_tick_length(struct SOUND *snd)
{
	if (snd->length_enabled)
	{
		snd->length = (snd->length + 1) & snd->length_mask;
		if (snd->length == 0)
		{
			snd->on = false;
			snd->length_counting = false;
		}
	}
}


static INT32 gb_calculate_next_sweep(struct SOUND *snd)
{
	INT32 new_frequency;
	snd->sweep_neg_mode_used = (snd->sweep_direction < 0);
	new_frequency = snd->frequency + snd->sweep_direction * (snd->frequency >> snd->sweep_shift);

	if (new_frequency > 0x7FF)
	{
		snd->on = false;
	}

	return new_frequency;
}


static void gb_apply_next_sweep(struct SOUND *snd)
{
	INT32 new_frequency = gb_calculate_next_sweep(snd);

	if (snd->on && snd->sweep_shift > 0)
	{
		snd->frequency = new_frequency;
		snd->reg[3] = snd->frequency & 0xFF;
	}
}


static void gb_tick_sweep(struct SOUND *snd)
{
	snd->sweep_count = (snd->sweep_count - 1) & 0x07;
	if (snd->sweep_count == 0)
	{
		snd->sweep_count = snd->sweep_time;

		if (snd->sweep_enabled && snd->sweep_time > 0)
		{
			gb_apply_next_sweep(snd);
			gb_calculate_next_sweep(snd);
		}
	}
}


static void gb_tick_envelope(struct SOUND *snd)
{
	if (snd->envelope_enabled)
	{
		snd->envelope_count = (snd->envelope_count - 1) & 0x07;

		if (snd->envelope_count == 0)
		{
			snd->envelope_count = snd->envelope_time;

			if (snd->envelope_count)
			{
				INT8 new_envelope_value = snd->envelope_value + snd->envelope_direction;

				if (new_envelope_value >= 0 && new_envelope_value <= 15)
				{
					snd->envelope_value = new_envelope_value;
				}
				else
				{
					snd->envelope_enabled = false;
				}
			}
		}
	}
}


INLINE bool gb_dac_enabled(struct SOUND *snd)
{
	return (snd->channel != 3) ? snd->reg[2] & 0xF8 : snd->reg[0] & 0x80;
}


static void gb_update_square_channel(struct SOUND *snd, UINT32 cycles)
{
	if (snd->on)
	{
		UINT16 distance;
		// compensate for leftover cycles
		snd->cycles_left += cycles;
		if (snd->cycles_left <= 0)
			return;

		cycles = snd->cycles_left >> 2;
		snd->cycles_left &= 3;
		distance = 0x800 - snd->frequency_counter;
		if (cycles >= distance)
		{
			UINT32 counter;
			cycles -= distance;
			distance = 0x800 - snd->frequency;
			counter = 1 + cycles / distance;

			snd->duty_count = (snd->duty_count + counter) & 0x07;
			snd->signal = wave_duty_table[snd->duty][snd->duty_count];

			snd->frequency_counter = snd->frequency + cycles % distance;
		}
		else
		{
			snd->frequency_counter += cycles;
		}
	}
}


static void gb_update_wave_channel(gb_sound_t *gb, struct SOUND *snd, UINT32 cycles)
{
	if (snd->on)
	{
		// compensate for leftover cycles
		snd->cycles_left += cycles;

		while (snd->cycles_left >= 2)
		{
			snd->cycles_left -= 2;

			// Calculate next state
			snd->frequency_counter = (snd->frequency_counter + 1) & 0x7ff;
			snd->sample_reading = false;
			if (gb->gbMode == GBMODE_DMG && snd->frequency_counter == 0x7ff)
				snd->offset = (snd->offset + 1) & 0x1F;
			if (snd->frequency_counter == 0)
			{
				// Read next sample
				snd->sample_reading = true;
				if (gb->gbMode == GBMODE_CGB04)
					snd->offset = (snd->offset + 1) & 0x1f;
				snd->current_sample = gb->snd_regs[AUD3W0 + (snd->offset/2)];
				if (!(snd->offset & 0x01))
				{
					snd->current_sample >>= 4;
				}
				snd->current_sample = (snd->current_sample & 0x0f) - 8;
				if (gb->BoostWaveChn)
					snd->current_sample <<= 1;

				snd->signal = snd->level ? snd->current_sample / (1 << (snd->level - 1)) : 0;

				// Reload frequency counter
				snd->frequency_counter = snd->frequency;
			}
		}
	}
}


static void gb_update_noise_channel(gb_sound_t *gb, struct SOUND *snd, UINT32 cycles)
{
	UINT32 period = gb_noise_period_cycles(gb);
	snd->cycles_left += cycles;
	while (snd->cycles_left >= period)
	{
		UINT16 feedback;
		snd->cycles_left -= period;

		// Using a Polynomial Counter (aka Linear Feedback Shift Register)
		// Mode 4 has a 15 bit counter so we need to shift the bits around accordingly.
		feedback = ((snd->noise_lfsr >> 1) ^ snd->noise_lfsr) & 1;
		snd->noise_lfsr = (snd->noise_lfsr >> 1) | (feedback << 14);
		if (snd->noise_short)
		{
			snd->noise_lfsr = (snd->noise_lfsr & ~(1 << 6)) | (feedback << 6);
		}
		snd->signal = (snd->noise_lfsr & 1) ? -1 : 1;
	}
}


static void gb_update_state(gb_sound_t *gb, UINT32 cycles)
{
	UINT32 old_cycles;

	if (!gb->snd_control.on)
		return;

	old_cycles = gb->snd_control.cycles;
	gb->snd_control.cycles += cycles;

	if ((old_cycles / FRAME_CYCLES) != (gb->snd_control.cycles / FRAME_CYCLES))
	{
		// Left over cycles in current frame
		UINT32 cycles_current_frame = FRAME_CYCLES - (old_cycles & (FRAME_CYCLES - 1));

		gb_update_square_channel(&gb->snd_1, cycles_current_frame);
		gb_update_square_channel(&gb->snd_2, cycles_current_frame);
		gb_update_wave_channel(gb, &gb->snd_3, cycles_current_frame);
		gb_update_noise_channel(gb, &gb->snd_4, cycles_current_frame);

		cycles -= cycles_current_frame;

		// Switch to next frame
		switch ((gb->snd_control.cycles / FRAME_CYCLES) & 0x07)
		{
		case 0:
			// length
			gb_tick_length(&gb->snd_1);
			gb_tick_length(&gb->snd_2);
			gb_tick_length(&gb->snd_3);
			gb_tick_length(&gb->snd_4);
			break;
		case 2:
			// sweep
			gb_tick_sweep(&gb->snd_1);
			// length
			gb_tick_length(&gb->snd_1);
			gb_tick_length(&gb->snd_2);
			gb_tick_length(&gb->snd_3);
			gb_tick_length(&gb->snd_4);
			break;
		case 4:
			// length
			gb_tick_length(&gb->snd_1);
			gb_tick_length(&gb->snd_2);
			gb_tick_length(&gb->snd_3);
			gb_tick_length(&gb->snd_4);
			break;
		case 6:
			// sweep
			gb_tick_sweep(&gb->snd_1);
			// length
			gb_tick_length(&gb->snd_1);
			gb_tick_length(&gb->snd_2);
			gb_tick_length(&gb->snd_3);
			gb_tick_length(&gb->snd_4);
			break;
		case 7:
			// update envelope
			gb_tick_envelope(&gb->snd_1);
			gb_tick_envelope(&gb->snd_2);
			gb_tick_envelope(&gb->snd_4);
			break;
		}
	}

	gb_update_square_channel(&gb->snd_1, cycles);
	gb_update_square_channel(&gb->snd_2, cycles);
	gb_update_wave_channel(gb, &gb->snd_3, cycles);
	gb_update_noise_channel(gb, &gb->snd_4, cycles);
}


INLINE UINT32 gb_noise_period_cycles(gb_sound_t *gb)
{
	static const UINT32 divisor[8] = { 8, 16,32, 48, 64, 80, 96, 112 };
	return divisor[gb->snd_4.reg[3] & 7] << (gb->snd_4.reg[3] >> 4);
}


static void gameboy_update(void *chip, UINT32 samples, DEV_SMPL **outputs)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	DEV_SMPL sample, left, right;
	UINT32 i;

	for (i = 0; i < samples; i++)
	{
		left = right = 0;

		RC_STEP(&gb->cycleCntr);
		gb_update_state(gb, RC_GET_VAL(&gb->cycleCntr));
		RC_MASK(&gb->cycleCntr);

		/* Mode 1 - Wave with Envelope and Sweep */
		if (gb->snd_1.on && !gb->snd_1.Muted)
		{
			sample = gb->snd_1.signal * gb->snd_1.envelope_value;

			if (gb->snd_control.mode1_left)
				left += sample;
			if (gb->snd_control.mode1_right)
				right += sample;
		}

		/* Mode 2 - Wave with Envelope */
		if (gb->snd_2.on && !gb->snd_2.Muted)
		{
			sample = gb->snd_2.signal * gb->snd_2.envelope_value;
			if (gb->snd_control.mode2_left)
				left += sample;
			if (gb->snd_control.mode2_right)
				right += sample;
		}

		/* Mode 3 - Wave patterns from WaveRAM */
		if (gb->snd_3.on && !gb->snd_3.Muted)
		{
			sample = gb->snd_3.signal;
			if (gb->snd_control.mode3_left)
				left += sample;
			if (gb->snd_control.mode3_right)
				right += sample;
		}

		/* Mode 4 - Noise with Envelope */
		if (gb->snd_4.on && !gb->snd_4.Muted)
		{
			sample = gb->snd_4.signal * gb->snd_4.envelope_value;
			if (gb->snd_control.mode4_left)
				left += sample;
			if (gb->snd_control.mode4_right)
				right += sample;
		}

		/* Adjust for master volume */
		left *= gb->snd_control.vol_left;
		right *= gb->snd_control.vol_right;

		/* pump up the volume */
		left <<= 6;
		right <<= 6;

		/* Update the buffers */
		outputs[0][i] = left;
		outputs[1][i] = right;
	}

	gb->snd_regs[NR52] = (gb->snd_regs[NR52]&0xf0) | gb->snd_1.on | (gb->snd_2.on << 1) | (gb->snd_3.on << 2) | (gb->snd_4.on << 3);
}


static UINT8 device_start_gameboy_sound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	gb_sound_t *gb;

	gb = (gb_sound_t *)calloc(1, sizeof(gb_sound_t));
	if (gb == NULL)
		return 0xFF;

	gb->rate = cfg->clock / 64;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, gb->rate, cfg->smplRate);

	gb->gbMode = (cfg->flags & 0x01) ? GBMODE_CGB04 : GBMODE_DMG;
	RC_SET_RATIO(&gb->cycleCntr, cfg->clock, gb->rate);

	gameboy_sound_set_mute_mask(gb, 0x00);
	gb->BoostWaveChn = 0x00;
	gb->NoWaveCorrupt = 0x00;
	gb->LegacyMode = 0x00;

	gb->_devData.chipInf = gb;
	INIT_DEVINF(retDevInf, &gb->_devData, gb->rate, &devDef);

	return 0x00;
}

static void device_stop_gameboy_sound(void *chip)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	
	free(gb);
	
	return;
}

static void device_reset_gameboy_sound(void *chip)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	UINT32 muteMask;

	muteMask = gameboy_sound_get_mute_mask(gb);

	RC_RESET(&gb->cycleCntr);

	memset(&gb->snd_1, 0, sizeof(gb->snd_1));
	memset(&gb->snd_2, 0, sizeof(gb->snd_2));
	memset(&gb->snd_3, 0, sizeof(gb->snd_3));
	memset(&gb->snd_4, 0, sizeof(gb->snd_4));

	gameboy_sound_set_mute_mask(gb, muteMask);

	gb->snd_1.channel = 1;
	gb->snd_1.length_mask = 0x3F;
	gb->snd_2.channel = 2;
	gb->snd_2.length_mask = 0x3F;
	gb->snd_3.channel = 3;
	gb->snd_3.length_mask = 0xFF;
	gb->snd_4.channel = 4;
	gb->snd_4.length_mask = 0x3F;

	gb_sound_w_internal(gb, NR52, 0x00);
	switch(gb->gbMode)
	{
	case GBMODE_DMG:
		gb->snd_regs[AUD3W0] = 0xac;
		gb->snd_regs[AUD3W1] = 0xdd;
		gb->snd_regs[AUD3W2] = 0xda;
		gb->snd_regs[AUD3W3] = 0x48;
		gb->snd_regs[AUD3W4] = 0x36;
		gb->snd_regs[AUD3W5] = 0x02;
		gb->snd_regs[AUD3W6] = 0xcf;
		gb->snd_regs[AUD3W7] = 0x16;
		gb->snd_regs[AUD3W8] = 0x2c;
		gb->snd_regs[AUD3W9] = 0x04;
		gb->snd_regs[AUD3WA] = 0xe5;
		gb->snd_regs[AUD3WB] = 0x2c;
		gb->snd_regs[AUD3WC] = 0xac;
		gb->snd_regs[AUD3WD] = 0xdd;
		gb->snd_regs[AUD3WE] = 0xda;
		gb->snd_regs[AUD3WF] = 0x48;
		break;
	case GBMODE_CGB04:
		gb->snd_regs[AUD3W0] = 0x00;
		gb->snd_regs[AUD3W1] = 0xFF;
		gb->snd_regs[AUD3W2] = 0x00;
		gb->snd_regs[AUD3W3] = 0xFF;
		gb->snd_regs[AUD3W4] = 0x00;
		gb->snd_regs[AUD3W5] = 0xFF;
		gb->snd_regs[AUD3W6] = 0x00;
		gb->snd_regs[AUD3W7] = 0xFF;
		gb->snd_regs[AUD3W8] = 0x00;
		gb->snd_regs[AUD3W9] = 0xFF;
		gb->snd_regs[AUD3WA] = 0x00;
		gb->snd_regs[AUD3WB] = 0xFF;
		gb->snd_regs[AUD3WC] = 0x00;
		gb->snd_regs[AUD3WD] = 0xFF;
		gb->snd_regs[AUD3WE] = 0x00;
		gb->snd_regs[AUD3WF] = 0xFF;
		break;
	}

	return;
}

static void gameboy_sound_set_mute_mask(void *chip, UINT32 MuteMask)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	
	gb->snd_1.Muted = (MuteMask >> 0) & 0x01;
	gb->snd_2.Muted = (MuteMask >> 1) & 0x01;
	gb->snd_3.Muted = (MuteMask >> 2) & 0x01;
	gb->snd_4.Muted = (MuteMask >> 3) & 0x01;
	
	return;
}

static UINT32 gameboy_sound_get_mute_mask(void *chip)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	UINT32 muteMask;
	
	muteMask =	(gb->snd_1.Muted << 0) |
				(gb->snd_2.Muted << 1) |
				(gb->snd_3.Muted << 2) |
				(gb->snd_4.Muted << 3);
	
	return muteMask;
}

static void gameboy_sound_set_options(void *chip, UINT32 Flags)
{
	gb_sound_t *gb = (gb_sound_t *)chip;
	
	gb->BoostWaveChn = (Flags & 0x01) >> 0;
	gb->NoWaveCorrupt = (Flags & 0x02) >> 1;
	gb->LegacyMode = (Flags & 0x80) >> 7;
	
	return;
}
