// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi, Alex W. Jackson
/*********************************************************

    Konami 053260 KDSC

    The 053260 is a four voice PCM/ADPCM sound chip that
    also incorporates four 8-bit ports for communication
    between a main CPU and audio CPU. The chip's output
    is compatible with a YM3012 DAC, and it has a digital
    auxiliary input compatible with the output of a YM2151.
    Some games (e.g. Simpsons) only connect one channel of
    the YM2151, but others (e.g. Thunder Cross II) connect
    both channels for stereo mixing.

    The 053260 has a 21-bit address bus and 8-bit data bus
    to ROM, allowing it to access up to 2 megabytes of
    sample data. Sample data can be either signed 8-bit
    PCM or a custom 4-bit ADPCM format. It is possible for
    two 053260 chips to share access to the same ROMs
    (used by Over Drive)

    The 053260 has separate address and data buses to the
    audio CPU controlling it and to the main CPU. Both data
    buses are 8 bit. The audio CPU address bus has 6 lines
    (64 addressable registers, but fewer than 48 are
    actually used) while the main CPU "bus" has only 1 line
    (2 addressable registers). All registers on the audio
    CPU side seem to be either read-only or write-only,
    although some games write 0 to all the registers in a
    loop at startup (including otherwise read-only or
    entirely unused registers).
    On the main CPU side, reads and writes to the same
    address access different communication ports.

    The sound data ROMs of Simpsons and Vendetta have
    "headers" listing all the samples in the ROM, their
    formats ("PCM" or "KADPCM"), start and end addresses.
    The header data doesn't seem to be used by the hardware
    (none of the other games have headers) but provides
    useful information about the chip.

    2004-02-28 (Oliver Achten)
    Fixed ADPCM decoding. Games sound much better now.

    2014-10-06 (Alex W. Jackson)
    Rewrote from scratch in C++; implemented communication
    ports properly; used the actual up counters instead of
    converting to fractional sample position; fixed ADPCM
    decoding bugs; added documentation.


*********************************************************/

#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../logging.h"
#include "../RatioCntr.h"
#include "k053260.h"

static void k053260_update(void* param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_k053260(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_k053260(void* chip);
static void device_reset_k053260(void* chip);

static UINT8 k053260_main_read(void* chip, UINT8 offset);
static void k053260_main_write(void* chip, UINT8 offset, UINT8 data);
static UINT8 k053260_read(void* chip, UINT8 offset);
static void k053260_write(void* chip, UINT8 offset, UINT8 data);

static void k053260_alloc_rom(void* chip, UINT32 memsize);
static void k053260_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);
static void k053260_set_mute_mask(void* chip, UINT32 MuteMask);
static void k053260_set_log_cb(void* chip, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, k053260_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, k053260_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, k053260_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, k053260_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, k053260_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"K053260", "MAME", FCC_MAME,
	
	device_start_k053260,
	device_stop_k053260,
	device_reset_k053260,
	k053260_update,
	
	NULL,	// SetOptionBits
	k053260_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	k053260_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};
const DEV_DEF* devDefList_K053260[] =
{
	&devDef,
	NULL
};


#define LOG 0

#define CLOCKS_PER_SAMPLE 64


// Pan multipliers.  Set according to integer angles in degrees, amusingly.
// Exact precision hard to know, the floating point-ish output format makes
// comparisons iffy.  So we used a 1.16 format.
static const int pan_mul[8][2] =
{
    {     0,     0 }, // No sound for pan 0
    { 65536,     0 }, //  0 degrees
    { 59870, 26656 }, // 24 degrees
    { 53684, 37950 }, // 35 degrees
    { 46341, 46341 }, // 45 degrees
    { 37950, 53684 }, // 55 degrees
    { 26656, 59870 }, // 66 degrees
    {     0, 65536 }  // 90 degrees
};


typedef struct _k053260_state k053260_state;

// per voice state
typedef struct
{
	// pointer to owning device
	k053260_state *device;

	// live state
	UINT32 position;
	INT32  pan_volume[2];
	UINT16 counter;
	INT8   output;
	UINT8  playing;

	// per voice registers
	UINT32 start;
	UINT16 length;
	UINT16 pitch;
	UINT8  volume;

	// bit packed registers
	UINT8  pan;
	UINT8  loop;
	UINT8  kadpcm;
	UINT8  reverse;
	UINT8  Muted;
} KDSC_Voice;

INLINE void KDSC_voice_start(KDSC_Voice* voice, k053260_state *device);
INLINE void KDSC_voice_reset(KDSC_Voice* voice);
INLINE void KDSC_set_register(KDSC_Voice* voice, UINT8 offset, UINT8 data);
INLINE void KDSC_set_loop(KDSC_Voice* voice, UINT8 data);
INLINE void KDSC_set_kadpcm(KDSC_Voice* voice, UINT8 data);
INLINE void KDSC_set_reverse(KDSC_Voice* voice, UINT8 data);
INLINE void KDSC_set_pan(KDSC_Voice* voice, UINT8 data);
INLINE void KDSC_update_pan_volume(KDSC_Voice* voice);
INLINE void KDSC_key_on(k053260_state* info, KDSC_Voice* voice);
INLINE void KDSC_key_off(KDSC_Voice* voice);
INLINE void KDSC_play(KDSC_Voice* voice, DEV_SMPL *outputs, UINT16 cycles);
INLINE UINT8 KDSC_read_rom(KDSC_Voice* voice);
INLINE UINT8 k053260_rom_data(k053260_state* info, UINT32 offs);

struct _k053260_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	// live state
	UINT8           portdata[4];
	UINT8           keyon;
	UINT8           mode;

	KDSC_Voice      voice[4];

	UINT8           *rom;
	UINT32          rom_size;
	UINT32          rom_mask;

	RATIO_CNTR      cycleCntr;
};

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

static UINT8 device_start_k053260(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	k053260_state *info;
	UINT32 rate;
	int i;

	info = (k053260_state *)calloc(1, sizeof(k053260_state));
	if (info == NULL)
		return 0xFF;

	info->rom = NULL;
	info->rom_size = 0x00;
	info->rom_mask = 0x00;

	for (i = 0; i < 4; i++)
		KDSC_voice_start(&info->voice[i], info);

	rate = cfg->clock / CLOCKS_PER_SAMPLE;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);

	RC_SET_RATIO(&info->cycleCntr, cfg->clock, rate);

	k053260_set_mute_mask(info, 0x00);

	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, rate, &devDef);

	return 0x00;
}


static void device_stop_k053260(void* chip)
{
	k053260_state *info = (k053260_state *)chip;

	free(info->rom);
	free(info);

	return;
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

static void device_reset_k053260(void* chip)
{
	k053260_state *info = (k053260_state *)chip;
	int i;

	memset(info->portdata, 0, sizeof(info->portdata));

	info->keyon = 0;
	info->mode = 0;

	for (i = 0; i < 4; i++)
		KDSC_voice_reset(&info->voice[i]);

	RC_RESET(&info->cycleCntr);

	return;
}


static UINT8 k053260_main_read(void* chip, UINT8 offset)
{
	k053260_state *info = (k053260_state *)chip;

	// sub-to-main ports
	return info->portdata[2 + (offset & 1)];
}


static void k053260_main_write(void* chip, UINT8 offset, UINT8 data)
{
	k053260_state *info = (k053260_state *)chip;

	// main-to-sub ports
	info->portdata[offset & 1] = data;
}


static UINT8 k053260_read(void* chip, UINT8 offset)
{
	k053260_state *info = (k053260_state *)chip;
	UINT8 ret;
	int i;

	offset &= 0x3f;
	ret = 0;

	switch (offset)
	{
		case 0x00: // main-to-sub ports
		case 0x01:
			ret = info->portdata[offset];
			break;

		case 0x29: // voice status
			for (i = 0; i < 4; i++)
				ret |= info->voice[i].playing << i;
			break;

		case 0x2e: // read ROM
			if (info->mode & 1)
				ret = KDSC_read_rom(&info->voice[0]);
			else
				emu_logf(&info->logger, DEVLOG_WARN, "Attempting to read ROM without mode bit set\n");
			break;

		default:
			emu_logf(&info->logger, DEVLOG_DEBUG, "Read from unknown register %02x\n", offset);
	}
	return ret;
}


static void k053260_write(void* chip, UINT8 offset, UINT8 data)
{
	k053260_state *info = (k053260_state *)chip;
	int i;

	offset &= 0x3f;

	// per voice registers
	if ((offset >= 0x08) && (offset <= 0x27))
	{
		KDSC_set_register(&info->voice[(offset - 8) / 8], offset, data);
		return;
	}

	switch (offset)
	{
		// 0x00 and 0x01 are read registers

		case 0x02: // sub-to-main ports
		case 0x03:
			info->portdata[offset] = data;
			break;

		// 0x04 through 0x07 seem to be unused

		case 0x28: // key on/off and reverse
		{
			UINT8 rising_edge = data & ~info->keyon;

			for (i = 0; i < 4; i++)
			{
				KDSC_set_reverse(&info->voice[i], (data >> (i | 4)) & 1);

				if (rising_edge & (1 << i))
					KDSC_key_on(info, &info->voice[i]);
				else if (!(data & (1 << i)))
					KDSC_key_off(&info->voice[i]);
			}
			info->keyon = data;
			break;
		}

		// 0x29 is a read register

		case 0x2a: // loop and pcm/adpcm select
			for (i = 0; i < 4; i++)
			{
				KDSC_set_loop(&info->voice[i], (data >> i) & 1);
				KDSC_set_kadpcm(&info->voice[i], (data >> (i | 4)) & 1);
			}
			break;

		// 0x2b seems to be unused

		case 0x2c: // pan, voices 0 and 1
			KDSC_set_pan(&info->voice[0], data);
			KDSC_set_pan(&info->voice[1], data >> 3);
			break;

		case 0x2d: // pan, voices 2 and 3
			KDSC_set_pan(&info->voice[2], data);
			KDSC_set_pan(&info->voice[3], data >> 3);
			break;

		// 0x2e is a read register

		case 0x2f: // control
			info->mode = data;
			// bit 0 = enable ROM read from register 0x2e
			// bit 1 = enable sound output
			// bit 2 = enable aux input?
			//   (set by all games except Golfing Greats and Rollergames, both of which
			//    don't have a YM2151. Over Drive only sets it on one of the two chips)
			// bit 3 = aux input or ROM sharing related?
			//   (only set by Over Drive, and only on the same chip that bit 2 is set on)
			break;

		default:
			emu_logf(&info->logger, DEVLOG_DEBUG, "Write to unknown register %02x (data = %02x)\n",
					offset, data);
	}
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void k053260_update(void* param, UINT32 samples, DEV_SMPL **outputs)
{
	k053260_state *info = (k053260_state *)param;

	if ((info->mode & 2) && info->rom != NULL)
	{
		UINT32 i, j;

		for (j = 0; j < samples; j++)
		{
			DEV_SMPL buffer[2] = {0, 0};
			UINT16 cycles;

			RC_STEP(&info->cycleCntr);
			cycles = (UINT16)RC_GET_VAL(&info->cycleCntr);
			RC_MASK(&info->cycleCntr);

			for ( i = 0; i < 4; i++ )
			{
				if (info->voice[i].playing && !info->voice[i].Muted)
					KDSC_play(&info->voice[i], buffer, cycles);
			}

			outputs[0][j] = buffer[0];
			outputs[1][j] = buffer[1];
		}
	}
	else
	{
		memset( outputs[0], 0, samples * sizeof(*outputs[0]));
		memset( outputs[1], 0, samples * sizeof(*outputs[1]));
	}
}


//**************************************************************************
//  KDSC_Voice - one of the four voices
//**************************************************************************

INLINE void KDSC_voice_start(KDSC_Voice* voice, k053260_state *device)
{
	voice->device = device;

	KDSC_voice_reset(voice);
}

INLINE void KDSC_voice_reset(KDSC_Voice* voice)
{
	voice->position = 0;
	voice->counter = 0;
	voice->output = 0;
	voice->playing = 0;
	voice->start = 0;
	voice->length = 0;
	voice->pitch = 0;
	voice->volume = 0;
	voice->pan = 0;
	voice->loop = 0;
	voice->kadpcm = 0;
	voice->reverse = 0;
	KDSC_update_pan_volume(voice);
}

INLINE void KDSC_set_register(KDSC_Voice* voice, UINT8 offset, UINT8 data)
{
	switch (offset & 0x7)
	{
		case 0: // pitch, lower 8 bits
			voice->pitch = (voice->pitch & 0x0f00) | data;
			break;
		case 1: // pitch, upper 4 bits
			voice->pitch = (voice->pitch & 0x00ff) | ((data << 8) & 0x0f00);
			break;
		case 2: // length, lower 8 bits
			voice->length = (voice->length & 0xff00) | data;
			break;
		case 3: // length, upper 8 bits
			voice->length = (voice->length & 0x00ff) | (data << 8);
			break;
		case 4: // start, lower 8 bits
			voice->start = (voice->start & 0x1fff00) | data;
			break;
		case 5: // start, middle 8 bits
			voice->start = (voice->start & 0x1f00ff) | (data << 8);
			break;
		case 6: // start, upper 5 bits
			voice->start = (voice->start & 0x00ffff) | ((data << 16) & 0x1f0000);
			break;
		case 7: // volume, 7 bits
			voice->volume = data & 0x7f;
			KDSC_update_pan_volume(voice);
			break;
	}
}

INLINE void KDSC_set_loop(KDSC_Voice* voice, UINT8 data)
{
	voice->loop = data;
}

INLINE void KDSC_set_kadpcm(KDSC_Voice* voice, UINT8 data)
{
	voice->kadpcm = data;
}

INLINE void KDSC_set_reverse(KDSC_Voice* voice, UINT8 data)
{
	voice->reverse = data;
}

INLINE void KDSC_set_pan(KDSC_Voice* voice, UINT8 data)
{
	voice->pan = data & 0x7;
	KDSC_update_pan_volume(voice);
}

INLINE void KDSC_update_pan_volume(KDSC_Voice* voice)
{
	voice->pan_volume[0] = voice->volume * pan_mul[voice->pan][0];
	voice->pan_volume[1] = voice->volume * pan_mul[voice->pan][1];
}

INLINE void KDSC_key_on(k053260_state* info, KDSC_Voice* voice)
{
	voice->position = voice->kadpcm ? 1 : 0; // for kadpcm low bit is nybble offset, so must start at 1 due to preincrement
	voice->counter = 0xFFFF; // force update on next sound_stream_update
	voice->output = 0;
	voice->playing = 1;
	if (LOG) emu_logf(&info->logger, DEVLOG_TRACE, "start = %06x, length = %06x, pitch = %04x, vol = %02x:%x, loop = %s, reverse = %s, %s\n",
					voice->start, voice->length, voice->pitch, voice->volume, voice->pan,
					voice->loop ? "yes" : "no",
					voice->reverse ? "yes" : "no",
					voice->kadpcm ? "KADPCM" : "PCM");
}

INLINE void KDSC_key_off(KDSC_Voice* voice)
{
	voice->position = 0;
	voice->output = 0;
	voice->playing = 0;
}

INLINE void KDSC_play(KDSC_Voice* voice, DEV_SMPL *outputs, UINT16 cycles)
{
	if (voice->counter == 0xFFFF)	// "force update"?
		voice->counter = 0x1000;
	else
		voice->counter += cycles;

	while (voice->counter >= 0x1000)
	{
		INT32 bytepos;
		UINT8 romdata;

		voice->counter = voice->counter - 0x1000 + voice->pitch;

		bytepos = (INT32)(++voice->position >> (voice->kadpcm ? 1 : 0));
		/*
		Yes, _pre_increment. Playback must start 1 byte position after the
		start address written to the register, or else ADPCM sounds will
		have DC offsets (e.g. TMNT2 theme song) or will overflow and be
		distorted (e.g. various Vendetta sound effects)
		The "headers" in the Simpsons and Vendetta sound ROMs provide
		further evidence of this quirk (the start addresses listed in the
		ROM header are all 1 greater than the addresses the CPU writes
		into the register)
		*/
		if (bytepos > voice->length)
		{
			if (voice->loop)
			{
				voice->position = voice->output = bytepos = 0;
			}
			else
			{
				voice->playing = 0;
				return;
			}
		}

		romdata = k053260_rom_data(voice->device, voice->start + (voice->reverse ? -bytepos : +bytepos));

		if (voice->kadpcm)
		{
			static const INT8 kadpcm_table[] = {0,1,2,4,8,16,32,64,-128,-64,-32,-16,-8,-4,-2,-1};
			if (voice->position & 1) romdata >>= 4; // decode low nybble, then high nybble
			voice->output += kadpcm_table[romdata & 0xf];
		}
		else
		{
			voice->output = romdata;
		}
	}

	outputs[0] += (voice->output * voice->pan_volume[0]) >> 15;
	outputs[1] += (voice->output * voice->pan_volume[1]) >> 15;
}

INLINE UINT8 KDSC_read_rom(KDSC_Voice* voice)
{
	UINT32 offs = voice->start + voice->position;

	voice->position = (voice->position + 1) & 0xffff;

	return k053260_rom_data(voice->device, offs);
}

INLINE UINT8 k053260_rom_data(k053260_state* info, UINT32 offs)
{
	return info->rom[offs & info->rom_mask];
}

static void k053260_alloc_rom(void* chip, UINT32 memsize)
{
	k053260_state *info = (k053260_state *)chip;
	
	if (info->rom_size == memsize)
		return;
	
	info->rom = (UINT8*)realloc(info->rom, memsize);
	info->rom_size = memsize;
	info->rom_mask = pow2_mask(memsize);
	memset(info->rom, 0xFF, memsize);
	
	return;
}

static void k053260_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	k053260_state *info = (k053260_state *)chip;
	
	if (offset > info->rom_size)
		return;
	if (offset + length > info->rom_size)
		length = info->rom_size - offset;
	
	memcpy(info->rom + offset, data, length);
	
	return;
}


static void k053260_set_mute_mask(void* chip, UINT32 MuteMask)
{
	k053260_state *info = (k053260_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		info->voice[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void k053260_set_log_cb(void* chip, DEVCB_LOG func, void* param)
{
	k053260_state *info = (k053260_state *)chip;
	dev_logger_set(&info->logger, info, func, param);
	return;
}
