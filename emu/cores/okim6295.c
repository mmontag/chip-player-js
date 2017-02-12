/**********************************************************************************************
 *
 *   streaming ADPCM driver
 *   by Aaron Giles
 *
 *   Library to transcode from an ADPCM source to raw PCM.
 *   Written by Buffoni Mirko in 08/06/97
 *   References: various sources and documents.
 *
 *   HJB 08/31/98
 *   modified to use an automatically selected oversampling factor
 *   for the current sample rate
 *
 *   Mish 21/7/99
 *   Updated to allow multiple OKI chips with different sample rates
 *
 *   R. Belmont 31/10/2003
 *   Updated to allow a driver to use both MSM6295s and "raw" ADPCM voices (gcpinbal)
 *   Also added some error trapping for MAME_DEBUG builds
 *
 **********************************************************************************************/


/*
  Note about the playback frequency: the external clock is internally divided,
  depending on pin 7, by 132 (high) or 165 (low).
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// for memset
#include <math.h>

#include <stdtype.h>
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../EmuCores.h"
#include "okim6295.h"

typedef struct _okim6295_state okim6295_state;


static void okim6295_set_bank_base(okim6295_state *info, UINT32 base);
INLINE void okim6295_set_pin7(okim6295_state *info, UINT8 pin7);

static void okim6295_update(void* info, UINT32 samples, DEV_SMPL** outputs);
static UINT8 device_start_okim6295(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_okim6295(void* chipptr);
static void device_reset_okim6295(void *chip);

static UINT8 okim6295_r(void* chip, UINT8 offset);
static void okim6295_w(void* info, UINT8 offset, UINT8 data);

static void okim6295_alloc_rom(void* info, UINT32 memsize);
static void okim6295_write_rom(void* info, UINT32 offset, UINT32 length, const UINT8* data);
static void okim6295_set_mute_mask(void *info, UINT32 MuteMask);
static void okim6295_set_srchg_cb(void* chip, DEVCB_SRATE_CHG CallbackFunc, void* DataPtr);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, okim6295_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, okim6295_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, okim6295_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, okim6295_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"MSM6295", "MAME", FCC_MAME,
	
	device_start_okim6295,
	device_stop_okim6295,
	device_reset_okim6295,
	okim6295_update,
	
	NULL,	// SetOptionBits
	okim6295_set_mute_mask,
	NULL,	// SetPanning
	okim6295_set_srchg_cb,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};
const DEV_DEF* devDefList_OKIM6295[] =
{
	&devDef,
	NULL
};


/*
    To help the various custom ADPCM generators out there,
    the following routines may be used.
*/
struct adpcm_state
{
	INT16	signal;
	INT16	step;
};
static void reset_adpcm(struct adpcm_state *state);
static INT16 clock_adpcm(struct adpcm_state *state, UINT8 nibble);


/* struct describing a single playing ADPCM voice */
struct ADPCMVoice
{
	UINT8 playing;			/* 1 if we are actively playing */

	UINT32 base_offset;		/* pointer to the base memory location */
	UINT32 sample;			/* current sample number */
	UINT32 count;			/* total samples to play */

	struct adpcm_state adpcm;/* current ADPCM state */
	INT32 volume;			/* output volume */
	UINT8 Muted;
};

struct _okim6295_state
{
	void* chipInf;
	
	#define OKIM6295_VOICES		4
	struct ADPCMVoice voice[OKIM6295_VOICES];
	INT16 command;
	UINT32 bank_offs;
	UINT8 pin7_state;
	UINT8 pin7_initial;
	UINT8 nmk_mode;
	UINT8 nmk_bank[4];
	UINT32 master_clock;	// master clock frequency
	UINT32 initial_clock;
	
	UINT32	ROMSize;
	UINT8*	ROM;
	
	DEVCB_SRATE_CHG SmpRateFunc;
	void* SmpRateData;
};

/* step size index shift table */
static const int index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

/* lookup table for the precomputed difference */
static int diff_lookup[49*16];

/* volume lookup table. The manual lists only 9 steps, ~3dB per step. Given the dB values,
   that seems to map to a 5-bit volume control. Any volume parameter beyond the 9th index
   results in silent playback. */
static const int volume_table[16] =
{
	0x20,	//   0 dB
	0x16,	//  -3.2 dB
	0x10,	//  -6.0 dB
	0x0b,	//  -9.2 dB
	0x08,	// -12.0 dB
	0x06,	// -14.5 dB
	0x04,	// -18.0 dB
	0x03,	// -20.5 dB
	0x02,	// -24.0 dB
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};

/* tables computed? */
static int tables_computed = 0;

/**********************************************************************************************

     compute_tables -- compute the difference tables

***********************************************************************************************/

static void compute_tables(void)
{
	/* nibble to bit map */
	static const int nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	int step, nib;

	/* loop over all possible steps */
	for (step = 0; step <= 48; step++)
	{
		/* compute the step value */
		int stepval = (int)floor(16.0 * pow(11.0 / 10.0, (double)step));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}

	tables_computed = 1;
}



/**********************************************************************************************

     reset_adpcm -- reset the ADPCM stream

***********************************************************************************************/

static void reset_adpcm(struct adpcm_state *state)
{
	/* make sure we have our tables */
	if (!tables_computed)
		compute_tables();

	/* reset the signal/step */
	state->signal = -2;
	state->step = 0;
}



/**********************************************************************************************

     clock_adpcm -- clock the next ADPCM byte

***********************************************************************************************/

static INT16 clock_adpcm(struct adpcm_state *state, UINT8 nibble)
{
	state->signal += diff_lookup[state->step * 16 + (nibble & 15)];

	/* clamp to the maximum */
	if (state->signal > 2047)
		state->signal = 2047;
	else if (state->signal < -2048)
		state->signal = -2048;

	/* adjust the step size and clamp */
	state->step += index_shift[nibble & 7];
	if (state->step > 48)
		state->step = 48;
	else if (state->step < 0)
		state->step = 0;

	/* return the signal */
	return state->signal;
}



/**********************************************************************************************

     generate_adpcm -- general ADPCM decoding routine

***********************************************************************************************/

#define NMK_BNKTBLBITS	8
#define NMK_BNKTBLSIZE	(1 << NMK_BNKTBLBITS)	// 0x100
#define NMK_TABLESIZE	(4 * NMK_BNKTBLSIZE)	// 0x400
#define NMK_TABLEMASK	(NMK_TABLESIZE - 1)		// 0x3FF

#define NMK_BANKBITS	16
#define NMK_BANKSIZE	(1 << NMK_BANKBITS)		// 0x10000
#define NMK_BANKMASK	(NMK_BANKSIZE - 1)		// 0xFFFF
#define NMK_ROMBASE		(4 * NMK_BANKSIZE)		// 0x40000

static UINT8 memory_raw_read_byte(okim6295_state *chip, UINT32 offset)
{
	UINT32 CurOfs;
	
	if (! chip->nmk_mode)
	{
		CurOfs = chip->bank_offs | offset;
	}
	else
	{
		UINT8 BankID;
		
		if (offset < NMK_TABLESIZE && (chip->nmk_mode & 0x80))
		{
			// pages sample table
			BankID = offset >> NMK_BNKTBLBITS;
			CurOfs = offset & NMK_TABLEMASK;	// 0x3FF, not 0xFF
		}
		else
		{
			BankID = offset >> NMK_BANKBITS;
			CurOfs = offset & NMK_BANKMASK;
		}
		CurOfs |= (chip->nmk_bank[BankID & 0x03] << NMK_BANKBITS);
	}
	if (CurOfs < chip->ROMSize)
		return chip->ROM[CurOfs];
	else
		return 0x00;
}

static void generate_adpcm(okim6295_state *chip, struct ADPCMVoice *voice, DEV_SMPL *buffer, UINT32 samples)
{
	/* if this voice is active */
	if (voice->playing)
	{
		UINT32 base = voice->base_offset;
		UINT32 sample = voice->sample;
		UINT32 count = voice->count;
		UINT32 i;

		/* loop while we still have samples to generate */
		for (i = 0; i < samples; i++)
		{
			/* compute the new amplitude and update the current step */
			UINT8 nibble = memory_raw_read_byte(chip, base + sample / 2) >> (((sample & 1) << 2) ^ 4);

			/* output to the buffer, scaling by the volume */
			/* signal in range -2048..2047, volume in range 2..32 => signal * volume / 2 in range -32768..32767 */
			buffer[i] += clock_adpcm(&voice->adpcm, nibble) * voice->volume / 2;

			/* next! */
			if (++sample >= count)
			{
				voice->playing = 0;
				break;
			}
		}

		/* update the parameters */
		voice->sample = sample;
	}
}



/**********************************************************************************************
 *
 *  OKIM 6295 ADPCM chip:
 *
 *  Command bytes are sent:
 *
 *      1xxx xxxx = start of 2-byte command sequence, xxxxxxx is the sample number to trigger
 *      abcd vvvv = second half of command; one of the abcd bits is set to indicate which voice
 *                  the v bits seem to be volumed
 *
 *      0abc d000 = stop playing; one or more of the abcd bits is set to indicate which voice(s)
 *
 *  Status is read:
 *
 *      ???? abcd = one bit per voice, set to 0 if nothing is playing, or 1 if it is active
 *
***********************************************************************************************/


/**********************************************************************************************

     okim6295_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

static void okim6295_update(void* info, UINT32 samples, DEV_SMPL** outputs)
{
	okim6295_state *chip = (okim6295_state *)info;
	int i;

	memset(outputs[0], 0, samples * sizeof(*outputs[0]));

	for (i = 0; i < OKIM6295_VOICES; i++)
	{
		struct ADPCMVoice *voice = &chip->voice[i];
		if (! voice->Muted)
			generate_adpcm(chip, voice, outputs[0], samples);
	}
	
	memcpy(outputs[1], outputs[0], samples * sizeof(*outputs[0]));
}



/**********************************************************************************************

     DEVICE_START( okim6295 ) -- start emulation of an OKIM6295-compatible chip

***********************************************************************************************/

static UINT8 device_start_okim6295(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	okim6295_state *info;
	UINT32 divisor;

	info = (okim6295_state *)calloc(1, sizeof(okim6295_state));
	if (info == NULL)
		return 0xFF;
	
	compute_tables();

	info->command = -1;
	info->bank_offs = 0;
	info->nmk_mode = 0x00;
	memset(info->nmk_bank, 0x00, 4 * sizeof(UINT8));
	info->ROM = NULL;
	info->ROMSize = 0x00;

	info->initial_clock = cfg->clock;
	info->pin7_initial = cfg->flags;
	info->SmpRateFunc = NULL;

	info->master_clock = info->initial_clock;
	info->pin7_state = info->pin7_initial;
	divisor = info->pin7_state ? 132 : 165;

	okim6295_set_mute_mask(info, 0x00);
	
	info->chipInf = info;
	INIT_DEVINF(retDevInf, (DEV_DATA*)info, info->master_clock / divisor, &devDef);
	return 0x00;
}

static void device_stop_okim6295(void* chipptr)
{
	okim6295_state *chip = (okim6295_state *)chipptr;
	
	free(chip->ROM);
	free(chip);
	
	return;
}

/**********************************************************************************************

     DEVICE_RESET( okim6295 ) -- stop emulation of an OKIM6295-compatible chip

***********************************************************************************************/

static void device_reset_okim6295(void *chip)
{
	okim6295_state *info = (okim6295_state *)chip;
	UINT8 voice;
	
	info->master_clock = info->initial_clock;
	info->pin7_state = info->pin7_initial;
	info->command = -1;
	info->bank_offs = 0;
	info->nmk_mode = 0x00;
	memset(info->nmk_bank, 0x00, 4 * sizeof(UINT8));
	
	for (voice = 0; voice < OKIM6295_VOICES; voice++)
	{
		info->voice[voice].volume = 0;
		reset_adpcm(&info->voice[voice].adpcm);
		
		info->voice[voice].playing = 0;
	}
}



/**********************************************************************************************

     okim6295_set_bank_base -- set the base of the bank for a given voice on a given chip

***********************************************************************************************/

static void okim6295_set_bank_base(okim6295_state *info, UINT32 base)
{
	info->bank_offs = base;
}



/**********************************************************************************************

     okim6295_set_pin7 -- adjust pin 7, which controls the internal clock division

***********************************************************************************************/

static void okim6295_clock_changed(okim6295_state *info)
{
	UINT32 divisor;
	divisor = info->pin7_state ? 132 : 165;
	if (info->SmpRateFunc != NULL)
		info->SmpRateFunc(info->SmpRateData, info->master_clock / divisor);
}

INLINE void okim6295_set_pin7(okim6295_state *info, UINT8 pin7)
{
	info->pin7_state = pin7;
	okim6295_clock_changed(info);
}


/**********************************************************************************************

     okim6295_status_r -- read the status port of an OKIM6295-compatible chip

***********************************************************************************************/

static UINT8 okim6295_r(void* chip, UINT8 offset)
{
	okim6295_state *info = (okim6295_state *)chip;
	int i, result;

	result = 0xf0;	/* naname expects bits 4-7 to be 1 */

	/* set the bit to 1 if something is playing on a given channel */
	for (i = 0; i < OKIM6295_VOICES; i++)
	{
		struct ADPCMVoice *voice = &info->voice[i];

		/* set the bit if it's playing */
		if (voice->playing)
			result |= 1 << i;
	}

	return result;
}



/**********************************************************************************************

     okim6295_data_w -- write to the data port of an OKIM6295-compatible chip

***********************************************************************************************/

static void okim6295_write_command(okim6295_state *info, UINT8 data)
{
	/* if a command is pending, process the second half */
	if (info->command != -1)
	{
		UINT8 temp = data >> 4, i;
		UINT32 start, stop, base;

		/* the manual explicitly says that it's not possible to start multiple voices at the same time */
		if (temp != 0 && temp != 1 && temp != 2 && temp != 4 && temp != 8)
			printf("OKI6295 start %x contact MAMEDEV\n", temp);

		/* determine which voice(s) (voice is set by a 1 bit in the upper 4 bits of the second byte) */
		for (i = 0; i < OKIM6295_VOICES; i++, temp >>= 1)
		{
			if (temp & 1)
			{
				struct ADPCMVoice *voice = &info->voice[i];

				/* determine the start/stop positions */
				base = info->command * 8;

				start  = memory_raw_read_byte(info, base + 0) << 16;
				start |= memory_raw_read_byte(info, base + 1) << 8;
				start |= memory_raw_read_byte(info, base + 2) << 0;
				start &= 0x3ffff;

				stop  = memory_raw_read_byte(info, base + 3) << 16;
				stop |= memory_raw_read_byte(info, base + 4) << 8;
				stop |= memory_raw_read_byte(info, base + 5) << 0;
				stop &= 0x3ffff;

				/* set up the voice to play this sample */
				if (start < stop)
				{
					if (!voice->playing) /* fixes Got-cha and Steel Force */
					{
						voice->playing = 1;
						voice->base_offset = start;
						voice->sample = 0;
						voice->count = 2 * (stop - start + 1);

						/* also reset the ADPCM parameters */
						reset_adpcm(&voice->adpcm);
						voice->volume = volume_table[data & 0x0f];
					}
					else
					{
						// just displays warnings when seeking
						//logerror("OKIM6295: Voice %u requested to play sample %02x on non-stopped voice\n",i,info->command);
					}
				}
				/* invalid samples go here */
				else
				{
					logerror("OKIM6295: Voice %u requested to play invalid sample %02x\n",i,info->command);
					voice->playing = 0;
				}
			}
		}

		/* reset the command */
		info->command = -1;
	}

	/* if this is the start of a command, remember the sample number for next time */
	else if (data & 0x80)
	{
		info->command = data & 0x7f;
	}

	/* otherwise, see if this is a silence command */
	else
	{
		UINT8 temp = data >> 3, i;

		/* determine which voice(s) (voice is set by a 1 bit in bits 3-6 of the command */
		for (i = 0; i < OKIM6295_VOICES; i++, temp >>= 1)
		{
			if (temp & 1)
			{
				struct ADPCMVoice *voice = &info->voice[i];

				voice->playing = 0;
			}
		}
	}
}

static void okim6295_w(void* info, UINT8 offset, UINT8 data)
{
	okim6295_state *chip = (okim6295_state *)info;
	
	switch(offset)
	{
	case 0x00:
		okim6295_write_command(chip, data);
		break;
	case 0x08:
		chip->master_clock &= ~0x000000FF;
		chip->master_clock |= data <<  0;
		break;
	case 0x09:
		chip->master_clock &= ~0x0000FF00;
		chip->master_clock |= data <<  8;
		break;
	case 0x0A:
		chip->master_clock &= ~0x00FF0000;
		chip->master_clock |= data << 16;
		break;
	case 0x0B:
		chip->master_clock &= ~0xFF000000;
		chip->master_clock |= data << 24;
		okim6295_clock_changed(chip);
		break;
	case 0x0C:
		okim6295_set_pin7(chip, data);
		break;
	case 0x0E:	// NMK112 bank switch enable
		chip->nmk_mode = data;
		break;
	case 0x0F:
		okim6295_set_bank_base(chip, data << 18);
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		chip->nmk_bank[offset & 0x03] = data;
		break;
	}
	
	return;
}

static void okim6295_alloc_rom(void* info, UINT32 memsize)
{
	okim6295_state *chip = (okim6295_state *)info;
	
	if (chip->ROMSize == memsize)
		return;
	
	chip->ROM = (UINT8*)realloc(chip->ROM, memsize);
	chip->ROMSize = memsize;
	memset(chip->ROM, 0xFF, chip->ROMSize);
	
	return;
}

static void okim6295_write_rom(void* info, UINT32 offset, UINT32 length, const UINT8* data)
{
	okim6295_state *chip = (okim6295_state *)info;
	
	if (offset >= chip->ROMSize)
		return;
	if (offset + length > chip->ROMSize)
		length = chip->ROMSize - offset;
	
	memcpy(&chip->ROM[offset], data, length);
	
	return;
}


static void okim6295_set_mute_mask(void *info, UINT32 MuteMask)
{
	okim6295_state *chip = (okim6295_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < OKIM6295_VOICES; CurChn ++)
		chip->voice[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void okim6295_set_srchg_cb(void* chip, DEVCB_SRATE_CHG CallbackFunc, void* DataPtr)
{
	okim6295_state *info = (okim6295_state *)chip;
	
	// set Sample Rate Change Callback routine
	info->SmpRateFunc = CallbackFunc;
	info->SmpRateData = DataPtr;
	
	return;
}
