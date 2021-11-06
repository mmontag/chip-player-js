// license:BSD-3-Clause
// copyright-holders:Acho A. Tang,R. Belmont, Valley Bell
/*********************************************************

Irem GA20 PCM Sound Chip
80 pin QFP, label NANAO GA20 (Nanao Corporation was Irem's parent company)

TODO:
- It's not currently known whether this chip is stereo.
- Is sample position base(regs 0,1) used while sample is playing, or
  latched at key on? We've always emulated it the latter way.
  gunforc2 seems to be the only game updating the address regs sometimes
  while a sample is playing, but it doesn't seem intentional.
- What is the 2nd sample address for? Is it end(cut-off) address, or
  loop start address? Every game writes a value that's past sample end.
- All games write either 0 or 2 to reg #6, do other bits have any function?


Revisions:

04-15-2002 Acho A. Tang
- rewrote channel mixing
- added prelimenary volume and sample rate emulation

05-30-2002 Acho A. Tang
- applied hyperbolic gain control to volume and used
  a musical-note style progression in sample rate
  calculation(still very inaccurate)

02-18-2004 R. Belmont
- sample rate calculation reverse-engineered.
  Thanks to Fujix, Yasuhiro Ogawa, the Guru, and Tormod
  for real PCB samples that made this possible.

02-03-2007 R. Belmont
- Cleaned up faux x86 assembly.

09-25-2018 Valley Bell & co
- rewrote channel update to make data 0 act as sample terminator

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
#include "iremga20.h"


static void IremGA20_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_iremga20(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_iremga20(void *info);
static void device_reset_iremga20(void *info);

static UINT8 irem_ga20_r(void *info, UINT8 offset);
static void irem_ga20_w(void *info, UINT8 offset, UINT8 data);

static void iremga20_alloc_rom(void* info, UINT32 memsize);
static void iremga20_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data);

static void iremga20_set_mute_mask(void *info, UINT32 MuteMask);
static void iremga20_set_options(void *chip, UINT32 Flags);
static void iremga20_set_log_cb(void* info, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, irem_ga20_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, irem_ga20_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, iremga20_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, iremga20_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, iremga20_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"Irem GA20", "MAME", FCC_MAME,
	
	device_start_iremga20,
	device_stop_iremga20,
	device_reset_iremga20,
	IremGA20_update,
	
	iremga20_set_options,
	iremga20_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	iremga20_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_GA20[] =
{
	&devDef,
	NULL
};


#define MAX_VOL 256
#define RATE_SHIFT 16

struct IremGA20_channel_def
{
	UINT32 start;
	UINT32 end;
	UINT32 pos;
	UINT32 frac;
	UINT32 fracrate;
	UINT16 volume;
	UINT8 rate;
	UINT8 counter;
	UINT8 play;
	UINT8 Muted;
	
	INT8 smpl1;
	INT8 smpl2;
};

typedef struct _ga20_state ga20_state;
struct _ga20_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	UINT8 *rom;
	UINT32 rom_size;
	UINT8 regs[0x20];
	struct IremGA20_channel_def channel[4];
	
	UINT8 interpolate;
};


// helper function for fetching sample data
// Looking one sample ahead is required for interpolation. (which the actual chip probably doesn't do)
INLINE void irem_ga20_cache_samples(ga20_state *chip, struct IremGA20_channel_def* ch)
{
	if (! chip->rom[ch->pos])	// check for sample end marker
		ch->play = 0;
	else
		ch->smpl1 = chip->rom[ch->pos] - 0x80;
	if (chip->rom[ch->pos + 1])
		ch->smpl2 = chip->rom[ch->pos + 1] - 0x80;
	
	return;
}

static void IremGA20_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	ga20_state *chip = (ga20_state *)param;
	DEV_SMPL *outL, *outR;
	UINT32 i;
	int j;
	DEV_SMPL smpl_int;
	DEV_SMPL sampleout;
	struct IremGA20_channel_def* ch;

	outL = outputs[0];
	outR = outputs[1];
	if (chip->rom == NULL)
	{
		memset(outL, 0, samples * sizeof(DEV_SMPL));
		memset(outR, 0, samples * sizeof(DEV_SMPL));
		return;
	}

	for (i = 0; i < samples; i++)
	{
		sampleout = 0;

		for (j = 0; j < 4; j++)
		{
			ch = &chip->channel[j];
			if (ch->Muted || ! ch->play)
				continue;
			
			if (! chip->interpolate)
			{
				sampleout += ch->smpl1 * (INT32)ch->volume;
			}
			else
			{
				smpl_int = (ch->smpl1 * ((1 << RATE_SHIFT) - ch->frac) + ch->smpl2 * ch->frac);
				sampleout += (smpl_int * (INT32)ch->volume) >> RATE_SHIFT;
			}
			ch->frac += ch->fracrate;
			ch->counter ++;
			if (! ch->counter)
			{
				// advance position + reset counter on overflow
				ch->pos ++;
				ch->frac = 0;
				ch->counter = ch->rate;
				irem_ga20_cache_samples(chip, ch);
			}
		}

		sampleout >>= 2;
		outL[i] = sampleout;
		outR[i] = sampleout;
	}
}

static void irem_ga20_w(void *info, UINT8 offset, UINT8 data)
{
	ga20_state *chip = (ga20_state *)info;
	struct IremGA20_channel_def* ch;
	int channel;

	//emu_logf(&chip->logger, DEVLOG_DEBUG, Offset %02x, data %04x\n",offset,data);

	offset &= 0x1F;
	channel = offset >> 3;

	chip->regs[offset] = data;

	// channel regs:
	// 0,1: start address
	// 2,3: end? address
	// 4: rate
	// 5: volume
	// 6: control
	// 7: voice status (read-only)

	ch = &chip->channel[channel];
	switch (offset & 0x7)
	{
		case 0: /* start address low */
			ch->start = (ch->start&0xff000) | (data<<4);
			break;

		case 1: /* start address high */
			ch->start = (ch->start&0x00ff0) | (data<<12);
			break;

		case 2: /* end? address low */
			ch->end = (ch->end&0xff000) | (data<<4);
			break;

		case 3: /* end? address high */
			ch->end = (ch->end&0x00ff0) | (data<<12);
			break;

		case 4:
			ch->rate = data;
			ch->fracrate = (1 << RATE_SHIFT) / (0x100 - ch->rate);
			break;

		case 5: //AT: gain control
			ch->volume = (data * MAX_VOL) / (data + 10);
			break;

		case 6: //AT: this is always written 2(enabling both channels?)
			// d1: key on/off
			if (data & 2)
			{
				ch->play = 1;
				ch->pos = ch->start;
				ch->counter = ch->rate;
				ch->frac = 0;
				irem_ga20_cache_samples(chip, ch);
			}
			else
			{
				ch->play = 0;
			}

			// other: unknown/unused
			// possibilities are: loop flag, left/right speaker(stereo)
			break;
	}
}

static UINT8 irem_ga20_r(void *info, UINT8 offset)
{
	ga20_state *chip = (ga20_state *)info;
	int channel;

	offset &= 0x1F;
	channel = offset >> 3;

	switch (offset & 0x7)
	{
		case 7: // voice status. bit 0 is 1 if active. (routine around 0xccc in rtypeleo)
			return chip->channel[channel].play;

		default:
			emu_logf(&chip->logger, DEVLOG_DEBUG, "read unk. register %d, channel %d\n", offset & 0x7, channel);
			break;
	}

	return 0;
}

static void device_reset_iremga20(void *info)
{
	ga20_state *chip = (ga20_state *)info;
	int i;

	for( i = 0; i < 4; i++ ) {
		chip->channel[i].start = 0;
		chip->channel[i].end = 0;
		chip->channel[i].pos = 0;
		chip->channel[i].frac = 0;
		chip->channel[i].fracrate = 0;
		chip->channel[i].volume = 0;
		chip->channel[i].rate = 0;
		chip->channel[i].counter = 0;
		chip->channel[i].play = 0;
	}

	memset(chip->regs, 0x00, 0x20 * sizeof(UINT8));
}

static UINT8 device_start_iremga20(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	ga20_state *chip;

	chip = (ga20_state *)calloc(1, sizeof(ga20_state));
	if (chip == NULL)
		return 0xFF;

	/* Initialize our chip structure */
	chip->rom = NULL;
	chip->rom_size = 0x00;
	chip->interpolate = 0;

	iremga20_set_mute_mask(chip, 0x0);

	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, cfg->clock / 4, &devDef);

	return 0x00;
}

static void device_stop_iremga20(void *info)
{
	ga20_state *chip = (ga20_state *)info;
	
	free(chip->rom);
	free(chip);
	
	return;
}

static void iremga20_alloc_rom(void* info, UINT32 memsize)
{
	ga20_state *chip = (ga20_state *)info;
	
	if (chip->rom_size == memsize)
		return;
	
	chip->rom = (UINT8*)realloc(chip->rom, memsize);
	chip->rom_size = memsize;
	memset(chip->rom, 0xFF, memsize);
	
	return;
}

static void iremga20_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	ga20_state *chip = (ga20_state *)info;
	
	if (offset > chip->rom_size)
		return;
	if (offset + length > chip->rom_size)
		length = chip->rom_size - offset;
	
	memcpy(chip->rom + offset, data, length);
	
	return;
}


static void iremga20_set_mute_mask(void *info, UINT32 MuteMask)
{
	ga20_state *chip = (ga20_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		chip->channel[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void iremga20_set_options(void *info, UINT32 Flags)
{
	ga20_state *chip = (ga20_state *)info;
	
	chip->interpolate = Flags & OPT_GA20_INTERPOLATE;
	
	return;
}

static void iremga20_set_log_cb(void* info, DEVCB_LOG func, void* param)
{
	ga20_state *chip = (ga20_state *)info;
	dev_logger_set(&chip->logger, chip, func, param);
	return;
}
