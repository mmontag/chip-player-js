// license:BSD-3-Clause
// copyright-holders:Acho A. Tang,R. Belmont
/*********************************************************

Irem GA20 PCM Sound Chip

It's not currently known whether this chip is stereo.


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

*********************************************************/

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
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


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, irem_ga20_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, irem_ga20_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, iremga20_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, iremga20_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"Irem GA20", "MAME", FCC_MAME,
	
	device_start_iremga20,
	device_stop_iremga20,
	device_reset_iremga20,
	IremGA20_update,
	
	NULL,	// SetOptionBits
	iremga20_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_GA20[] =
{
	&devDef,
	NULL
};


#define MAX_VOL 256
#define RATE_SHIFT 24

struct IremGA20_channel_def
{
	UINT32 rate;
	UINT32 start;
	UINT32 pos;
	UINT32 frac;
	UINT32 end;
	UINT16 volume;
	UINT16 pan;
	//UINT16 effect;
	UINT8 play;
	UINT8 Muted;
};

typedef struct _ga20_state ga20_state;
struct _ga20_state
{
	DEV_DATA _devData;
	
	UINT8 *rom;
	UINT32 rom_size;
	UINT8 regs[0x20];
	struct IremGA20_channel_def channel[4];
};


static void IremGA20_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	ga20_state *chip = (ga20_state *)param;
	DEV_SMPL *outL, *outR;
	UINT32 i;
	int j;
	DEV_SMPL sampleout;
	struct IremGA20_channel_def* ch;

	outL = outputs[0];
	outR = outputs[1];

	for (i = 0; i < samples; i++)
	{
		sampleout = 0;

		for (j = 0; j < 4; j++)
		{
			ch = &chip->channel[j];
			if (ch->Muted || ! ch->play)
				continue;
			
			sampleout += (chip->rom[ch->pos] - 0x80) * (INT32)ch->volume;
			ch->frac += ch->rate;
			ch->pos += (ch->frac >> RATE_SHIFT);
			ch->frac &= ((1 << RATE_SHIFT) - 1);
			if (ch->pos >= ch->end - 0x20)
				ch->play = 0x00;
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

	//logerror("GA20:  Offset %02x, data %04x\n",offset,data);

	offset &= 0x1F;
	channel = offset >> 3;

	chip->regs[offset] = data;

	ch = &chip->channel[channel];
	switch (offset & 0x7)
	{
		case 0: /* start address low */
			ch->start = ((ch->start)&0xff000) | (data<<4);
			break;

		case 1: /* start address high */
			ch->start = ((ch->start)&0x00ff0) | (data<<12);
			break;

		case 2: /* end address low */
			ch->end = ((ch->end)&0xff000) | (data<<4);
			break;

		case 3: /* end address high */
			ch->end = ((ch->end)&0x00ff0) | (data<<12);
			break;

		case 4:
			ch->rate = (1 << RATE_SHIFT) / (256 - data);
			break;

		case 5: //AT: gain control
			ch->volume = (data * MAX_VOL) / (data + 10);
			break;

		case 6: //AT: this is always written 2(enabling both channels?)
			ch->play = data;
			ch->pos = ch->start;
			ch->frac = 0;
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
		case 7: // voice status.  bit 0 is 1 if active. (routine around 0xccc in rtypeleo)
			return chip->channel[channel].play ? 1 : 0;

		default:
			logerror("GA20: read unk. register %d, channel %d\n", offset & 0xf, channel);
			break;
	}

	return 0;
}

static void device_reset_iremga20(void *info)
{
	ga20_state *chip = (ga20_state *)info;
	int i;

	for( i = 0; i < 4; i++ ) {
		chip->channel[i].rate = 0;
		chip->channel[i].start = 0;
		chip->channel[i].pos = 0;
		chip->channel[i].frac = 0;
		chip->channel[i].end = 0;
		chip->channel[i].volume = 0;
		chip->channel[i].pan = 0;
		//chip->channel[i].effect = 0;
		chip->channel[i].play = 0;
	}

	memset(chip->regs, 0x00, 0x40 * sizeof(UINT8));
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
