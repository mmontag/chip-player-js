// license:BSD-3-Clause
// copyright-holders:Mirko Buffoni,Aaron Giles
/***************************************************************************

    okim6295.h

    OKIM 6295 ADCPM sound chip.

****************************************************************************

    Library to transcode from an ADPCM source to raw PCM.
    Written by Buffoni Mirko in 08/06/97
    References: various sources and documents.

    R. Belmont 31/10/2003
    Updated to allow a driver to use both MSM6295s and "raw" ADPCM voices
    (gcpinbal). Also added some error trapping for MAME_DEBUG builds

****************************************************************************

    OKIM 6295 ADPCM chip:

    Command bytes are sent:

        1xxx xxxx = start of 2-byte command sequence, xxxxxxx is the sample
                    number to trigger
        abcd vvvv = second half of command; one of the abcd bits is set to
                    indicate which voice the v bits seem to be volumed

        0abc d000 = stop playing; one or more of the abcd bits is set to
                    indicate which voice(s)

    Status is read:

        ???? abcd = one bit per voice, set to 0 if nothing is playing, or
                    1 if it is active

    OKI Semiconductor produced this chip in two package variants. The
    44-pin QFP version, MSM6295GS, is the original one and by far the more
    common of the two. The 42-pin DIP version, MSM6295VRS, omits A17 and
    RD, which limits its ROM addressing to one megabit instead of two.

***************************************************************************/


/*
  Note about the playback frequency: the external clock is internally divided,
  depending on pin 7, by 132 (high) or 165 (low).
*/


#include <stdlib.h>
#include <string.h>	// for memset
#include <math.h>

#include "../../stdtype.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../EmuCores.h"
#include "../logging.h"
#include "okim6295.h"
#include "okiadpcm.h"

typedef struct _okim6295_state okim6295_state;


static void okim6295_set_bank_base(okim6295_state *info, UINT32 base);
static UINT32 okim6295_get_rate(void* chip);
static void okim6295_set_clock(void *chip, UINT32 clock);
INLINE void okim6295_set_pin7(okim6295_state *info, UINT8 pin7);

static void okim6295_update(void* info, UINT32 samples, DEV_SMPL** outputs);
static UINT8 device_start_okim6295(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_okim6295(void* chipptr);
static void device_reset_okim6295(void *chip);

static UINT8 okim6295_r(void* chip, UINT8 offset);
static void okim6295_w(void* chip, UINT8 offset, UINT8 data);

static void okim6295_alloc_rom(void* info, UINT32 memsize);
static void okim6295_write_rom(void* info, UINT32 offset, UINT32 length, const UINT8* data);
static void okim6295_set_mute_mask(void *info, UINT32 MuteMask);
static void okim6295_set_srchg_cb(void* chip, DEVCB_SRATE_CHG CallbackFunc, void* DataPtr);
static void okim6295_set_log_cb(void* chip, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, okim6295_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, okim6295_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, okim6295_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, okim6295_alloc_rom},
	{RWF_CLOCK | RWF_WRITE, DEVRW_VALUE, 0, okim6295_set_clock},
	{RWF_SRATE | RWF_READ, DEVRW_VALUE, 0, okim6295_get_rate},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, okim6295_set_mute_mask},
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
	okim6295_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};
const DEV_DEF* devDefList_OKIM6295[] =
{
	&devDef,
	NULL
};


// a single voice
typedef struct _okim_voice
{
	oki_adpcm_state adpcm;          // current ADPCM state
	UINT8           playing;        // 1 if we are actively playing

	UINT32          base_offset;    // pointer to the base memory location
	UINT32          sample;         // current sample number
	UINT32          count;          // total samples to play

	INT32           volume;         // output volume
	UINT8           Muted;
} okim_voice;

struct _okim6295_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	// internal state
	#define OKIM6295_VOICES 4
	
	okim_voice voice[OKIM6295_VOICES];
	INT16 command;
	UINT32 bank_offs;
	UINT8 pin7_state;
	
	//const UINT8 s_volume_table[16];
	
	UINT8 pin7_initial;
	UINT8 nmk_mode;
	UINT8 nmk_bank[4];
	UINT32 master_clock;    // master clock frequency
	UINT8 clock_buffer[4];
	UINT32 initial_clock;
	
	UINT32  ROMSize;
	UINT8*  ROM;
	
	DEVCB_SRATE_CHG SmpRateFunc;
	void* SmpRateData;
};

/* volume lookup table. The manual lists only 9 steps, ~3dB per step. Given the dB values,
   that seems to map to a 5-bit volume control. Any volume parameter beyond the 9th index
   results in silent playback. */
static const int volume_table[16] =
{
	0x20,   //   0 dB
	0x16,   //  -3.2 dB
	0x10,   //  -6.0 dB
	0x0b,   //  -9.2 dB
	0x08,   // -12.0 dB
	0x06,   // -14.5 dB
	0x04,   // -18.0 dB
	0x03,   // -20.5 dB
	0x02,   // -24.0 dB
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};


INLINE UINT32 ReadLE32(const UINT8* buffer)
{
	return	(buffer[0] <<  0) | (buffer[1] <<  8) |
			(buffer[2] << 16) | (buffer[3] << 24);
}

INLINE void WriteLE32(UINT8* buffer, UINT32 value)
{
	buffer[0] = (value >>  0) & 0xFF;
	buffer[1] = (value >>  8) & 0xFF;
	buffer[2] = (value >> 16) & 0xFF;
	buffer[3] = (value >> 24) & 0xFF;
	return;
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

static void generate_adpcm(okim6295_state *chip, okim_voice *voice, DEV_SMPL *buffer, UINT32 samples)
{
	UINT32 i;

	// skip if not active
	if (!voice->playing || voice->Muted)
		return;

	// loop while we still have samples to generate
	for (i = 0; i < samples; i++)
	{
		// fetch the next sample byte
		UINT8 nibble = memory_raw_read_byte(chip, voice->base_offset + voice->sample / 2) >> (((voice->sample & 1) << 2) ^ 4);

		// output to the buffer, scaling by the volume
		// signal in range -2048..2047, volume in range 2..32 => signal * volume / 2 in range -32768..32767
		buffer[i] += oki_adpcm_clock(&voice->adpcm, nibble) * voice->volume / 2;

		// next!
		if (++voice->sample >= voice->count)
		{
			voice->playing = 0;
			break;
		}
	}
}



/**********************************************************************************************

     okim6295_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

static void okim6295_update(void* info, UINT32 samples, DEV_SMPL** outputs)
{
	okim6295_state *chip = (okim6295_state *)info;
	int i;

	// reset the output stream
	memset(outputs[0], 0, samples * sizeof(*outputs[0]));

	if (chip->ROM != NULL)
	{
		// iterate over voices and accumulate sample data
		for (i = 0; i < OKIM6295_VOICES; i++)
			generate_adpcm(chip, &chip->voice[i], outputs[0], samples);
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
	int voicenum;

	info = (okim6295_state *)calloc(1, sizeof(okim6295_state));
	if (info == NULL)
		return 0xFF;

	for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum++)
		oki_adpcm_init(&info->voice[voicenum].adpcm, NULL, NULL);

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
	WriteLE32(info->clock_buffer, info->master_clock);
	info->pin7_state = info->pin7_initial;
	divisor = info->pin7_state ? 132 : 165;

	okim6295_set_mute_mask(info, 0x00);
	
	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, info->master_clock / divisor, &devDef);
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
	WriteLE32(info->clock_buffer, info->master_clock);
	info->pin7_state = info->pin7_initial;
	if (info->SmpRateFunc != NULL)
		info->SmpRateFunc(info->SmpRateData, okim6295_get_rate(chip));
	
	info->command = -1;
	info->bank_offs = 0;
	info->nmk_mode = 0x00;
	memset(info->nmk_bank, 0x00, 4 * sizeof(UINT8));
	
	for (voice = 0; voice < OKIM6295_VOICES; voice++)
	{
		info->voice[voice].volume = 0;
		oki_adpcm_reset(&info->voice[voice].adpcm);
		
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

static UINT32 okim6295_get_rate(void* chip)
{
	okim6295_state *info = (okim6295_state *)chip;
	UINT32 divisor = info->pin7_state ? 132 : 165;
	return info->master_clock / divisor;
}

static void okim6295_set_clock(void *chip, UINT32 clock)
{
	okim6295_state *info = (okim6295_state *)chip;
	
	if (clock)
		info->master_clock = clock;	// set to parameter
	else
		info->master_clock = ReadLE32(info->clock_buffer);	// set to value from buffer
	if (info->SmpRateFunc != NULL)
		info->SmpRateFunc(info->SmpRateData, okim6295_get_rate(chip));
}

INLINE void okim6295_set_pin7(okim6295_state *info, UINT8 pin7)
{
	info->pin7_state = pin7;
	if (info->SmpRateFunc != NULL)
		info->SmpRateFunc(info->SmpRateData, okim6295_get_rate(info));
}


/**********************************************************************************************

     okim6295_status_r -- read the status port of an OKIM6295-compatible chip

***********************************************************************************************/

static UINT8 okim6295_r(void* chip, UINT8 offset)
{
	okim6295_state *info = (okim6295_state *)chip;
	int voicenum;
	UINT8 result;

	result = 0xf0;  // naname expects bits 4-7 to be 1

	// set the bit to 1 if something is playing on a given channel
	for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum++)
		if (info->voice[voicenum].playing)
			result |= 1 << voicenum;

	return result;
}



/**********************************************************************************************

     okim6295_data_w -- write to the data port of an OKIM6295-compatible chip

***********************************************************************************************/

static void okim6295_write_command(okim6295_state *info, UINT8 data)
{
	// if a command is pending, process the second half
	if (info->command != -1)
	{
		// the manual explicitly says that it's not possible to start multiple voices at the same time
		UINT8 voicemask = data >> 4;
		int voicenum;
		//if (voicemask != 0 && voicemask != 1 && voicemask != 2 && voicemask != 4 && voicemask != 8)
		//	printf("OKI6295 start %x contact MAMEDEV\n", voicemask);

		// determine which voice(s) (voice is set by a 1 bit in the upper 4 bits of the second byte)
		for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum++, voicemask >>= 1)
			if (voicemask & 1)
			{
				okim_voice *voice = &info->voice[voicenum];

				if (!voice->playing) // fixes Got-cha and Steel Force
				{
					UINT32 start, stop, base;
					// determine the start/stop positions
					base = info->command * 8;

					start  = memory_raw_read_byte(info, base + 0) << 16;
					start |= memory_raw_read_byte(info, base + 1) << 8;
					start |= memory_raw_read_byte(info, base + 2) << 0;
					start &= 0x3ffff;

					stop  = memory_raw_read_byte(info, base + 3) << 16;
					stop |= memory_raw_read_byte(info, base + 4) << 8;
					stop |= memory_raw_read_byte(info, base + 5) << 0;
					stop &= 0x3ffff;

					if (start < stop)
					{
						// set up the voice to play this sample
						voice->playing = 1;
						voice->base_offset = start;
						voice->sample = 0;
						voice->count = 2 * (stop - start + 1);

						// also reset the ADPCM parameters
						oki_adpcm_reset(&voice->adpcm);
						voice->volume = volume_table[data & 0x0f];
					}

					// invalid samples go here
					else
					{
						emu_logf(&info->logger, DEVLOG_DEBUG, "Voice %u requested to play invalid sample %02x\n",voicenum,info->command);
						voice->playing = 0;
					}
				}
				else
				{
					// just displays warnings when seeking
					//emu_logf(&info->logger, DEVLOG_DEBUG, "Voice %u requested to play sample %02x on non-stopped voice\n",voicenum,info->command);
				}
			}

		// reset the command
		info->command = -1;
	}

	// if this is the start of a command, remember the sample number for next time
	else if (data & 0x80)
	{
		info->command = data & 0x7f;
	}

	// otherwise, see if this is a silence command
	else
	{
		// determine which voice(s) (voice is set by a 1 bit in bits 3-6 of the command
		UINT8 voicemask = data >> 3;
		int voicenum;
		for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum++, voicemask >>= 1)
			if (voicemask & 1)
				info->voice[voicenum].playing = 0;
	}
}

static void okim6295_w(void* chip, UINT8 offset, UINT8 data)
{
	okim6295_state *info = (okim6295_state *)chip;
	
	switch(offset)
	{
	case 0x00:
		okim6295_write_command(info, data);
		break;
	case 0x08:
	case 0x09:
	case 0x0A:
		info->clock_buffer[offset & 0x03] = data;
		break;
	case 0x0B:
		info->clock_buffer[offset & 0x03] = data;
		okim6295_set_clock(chip, 0);	// refresh clock from clock_buffer
		if (info->SmpRateFunc != NULL)
			info->SmpRateFunc(info->SmpRateData, okim6295_get_rate(chip));
		break;
	case 0x0C:
		okim6295_set_pin7(info, data);
		break;
	case 0x0E:	// NMK112 bank switch enable
		info->nmk_mode = data;
		break;
	case 0x0F:
		okim6295_set_bank_base(info, data << 18);
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		info->nmk_bank[offset & 0x03] = data;
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

static void okim6295_set_log_cb(void* chip, DEVCB_LOG func, void* param)
{
	okim6295_state *info = (okim6295_state *)chip;
	dev_logger_set(&info->logger, info, func, param);
	return;
}
