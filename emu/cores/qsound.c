/***************************************************************************

  Capcom System QSound(tm)
  ========================

  Driver by Paul Leaman (paul@vortexcomputing.demon.co.uk)
        and Miguel Angel Horna (mahorna@teleline.es)

  A 16 channel stereo sample player.

  QSpace position is simulated by panning the sound in the stereo space.

  Register
  0  xxbb   xx = unknown bb = start high address
  1  ssss   ssss = sample start address
  2  pitch
  3  unknown (always 0x8000)
  4  loop offset from end address
  5  end
  6  master channel volume
  7  not used
  8  Balance (left=0x0110  centre=0x0120 right=0x0130)
  9  unknown (most fixed samples use 0 for this register)

  Many thanks to CAB (the author of Amuse), without whom this probably would
  never have been finished.

  If anybody has some information about this hardware, please send it to me
  to mahorna@teleline.es or 432937@cepsz.unizar.es.
  http://teleline.terra.es/personal/mahorna

***************************************************************************/

#ifdef _DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL
#include <math.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "qsound.h"


static void qsound_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_qsound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_qsound(void *info);
static void device_reset_qsound(void *info);

static void qsound_w(void *info, UINT8 offset, UINT8 data);
static UINT8 qsound_r(void *info, UINT8 offset);
static void qsound_set_command(void *info, UINT8 address, UINT16 data);

static void qsound_alloc_rom(void* info, UINT32 memsize);
static void qsound_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data);
static void qsound_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, qsound_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, qsound_r},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D16, 0, qsound_set_command},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, qsound_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, qsound_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"QSound", "MAME", FCC_MAME,
	
	device_start_qsound,
	device_stop_qsound,
	device_reset_qsound,
	qsound_update,
	
	NULL,	// SetOptionBits
	qsound_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_QSound[] =
{
	&devDef,
	NULL
};


/*
Debug defines
*/
#define VERBOSE  0
#define LOG(x) do { if (VERBOSE) logerror x; } while (0)


#define QSOUND_CLOCK    4000000   /* default 4MHz clock */
#define QSOUND_CLOCKDIV 166			 /* Clock divider */
#define QSOUND_CHANNELS 16

struct QSOUND_CHANNEL
{
	UINT32 bank;        // bank
	UINT32 address;     // start/cur address
	UINT16 loop;        // loop address
	UINT16 end;         // end address
	UINT32 freq;        // frequency
	UINT16 vol;         // master volume

	// work variables
	UINT8 enabled;      // key on / key off
	int lvol;           // left volume
	int rvol;           // right volume
	UINT32 step_ptr;    // current offset counter
	
	UINT8 Muted;
};

typedef struct _qsound_state qsound_state;
struct _qsound_state
{
	void* chipInf;
	
	struct QSOUND_CHANNEL channel[QSOUND_CHANNELS];
	
	UINT16 data;			/* register latch data */
	INT8 *sample_rom;		/* Q sound sample ROM */
	UINT32 sample_rom_length;

	int pan_table[33];		/* Pan volume table */
};

static UINT8 device_start_qsound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	qsound_state *chip;
	UINT32 clock;
	int i;

	chip = (qsound_state *)calloc(1, sizeof(qsound_state));
	if (chip == NULL)
		return 0xFF;
	
	clock = CHPCLK_CLOCK(cfg->clock);
	chip->sample_rom = NULL;
	chip->sample_rom_length = 0x00;

	/* Create pan table */
	for (i=0; i<33; i++)
		chip->pan_table[i]=(int)((256/sqrt(32.0)) * sqrt((double)i));
	
	// init sound regs
	memset(chip->channel, 0, sizeof(chip->channel));

	qsound_set_mute_mask(chip, 0x0000);

	chip->chipInf = chip;
	INIT_DEVINF(retDevInf, (DEV_DATA*)chip, clock / QSOUND_CLOCKDIV, &devDef);

	return 0x00;
}

static void device_stop_qsound(void *info)
{
	qsound_state *chip = (qsound_state *)info;
	free(chip->sample_rom);
	free(chip);
}

static void device_reset_qsound(void *info)
{
	qsound_state *chip = (qsound_state *)info;
	int adr;
	
	// TODO: save/restore muting
	// init sound regs
	memset(chip->channel, 0, sizeof(chip->channel));

	for (adr = 0x7f; adr >= 0; adr--)
		qsound_set_command(chip, adr, 0);
	for (adr = 0x80; adr < 0x90; adr++)
		qsound_set_command(chip, adr, 0x120);

	return;
}

static void qsound_w(void *info, UINT8 offset, UINT8 data)
{
	qsound_state *chip = (qsound_state *)info;
	switch (offset)
	{
		case 0:
			chip->data=(chip->data&0xff)|(data<<8);
			break;

		case 1:
			chip->data=(chip->data&0xff00)|data;
			break;

		case 2:
			qsound_set_command(chip, data, chip->data);
			break;

		default:
			//logerror("%s: unexpected qsound write to offset %d == %02X\n", device->machine().describe_context(), offset, data);
			logerror("QSound: unexpected qsound write to offset %d == %02X\n", offset, data);
			break;
	}
}

static UINT8 qsound_r(void *info, UINT8 offset)
{
	/* Port ready bit (0x80 if ready) */
	return 0x80;
}

static void qsound_set_command(void *info, UINT8 address, UINT16 data)
{
	qsound_state *chip = (qsound_state *)info;
	int ch = 0, reg = 0;

	// direct sound reg
	if (address < 0x80)
	{
		ch = address >> 3;
		reg = address & 0x07;
	}
	// >= 0x80 is probably for the dsp?
	else if (address < 0x90)
	{
		ch = address & 0x0F;
		reg = 8;
	}
	else if (address >= 0xba && address < 0xca)
	{
		ch = address - 0xba;
		reg=9;
	}
	else
	{
		/* Unknown registers */
		ch = 99;
		reg = 99;
	}

	switch (reg)
	{
		case 0:
			// bank, high bits unknown
			ch = (ch + 1) & 0x0f;	/* strange ... */
			chip->channel[ch].bank = (data & 0x7f) << 16;	// Note: The most recent MAME doesn't do "& 0x7F"
#ifdef _DEBUG
			if (data && !(data & 0x8000))
				printf("QSound Ch %u: Bank = %04x\n",ch,data);
#endif
			break;
		case 1:
			// start/cur address
			chip->channel[ch].address = data;
			break;
		case 2:
			// frequency
			chip->channel[ch].freq = data;
			break;
		case 3:
#ifdef _DEBUG
			if (chip->channel[ch].enabled && data != 0x8000)
				printf("QSound Ch %u: KeyOff (0x%04x)\n",ch,data);
#endif
			chip->channel[ch].enabled = (data & 0x8000) >> 15;
			chip->channel[ch].step_ptr = 0;
			break;
		case 4:
			// loop address
			chip->channel[ch].loop = data;
			break;
		case 5:
			// end address
			chip->channel[ch].end = data;
			break;
		case 6:
			// master volume
			chip->channel[ch].vol = data;
			break;
		case 7:
			// unused?
#ifdef MAME_DEBUG
			popmessage("UNUSED QSOUND REG 7=%04x",data);
#endif
			break;
		case 8:
			{
				// panning (left=0x0110, centre=0x0120, right=0x0130)
				// looks like it doesn't write other values than that
				int pan = (data & 0x3f) - 0x10;
				if (pan > 0x20)
					pan = 0x20;
				if (pan < 0)
					pan = 0;
				
				chip->channel[ch].rvol=chip->pan_table[pan];
				chip->channel[ch].lvol=chip->pan_table[0x20 - pan];
			}
			break;
		case 9:
			// unknown
			//logerror("QSOUND REG 9=%04x",data);
			break;
		default:
			//logerror("%s: write_data %02x = %04x\n", machine().describe_context(), address, data);
			break;
	}
	//LOG(("QSOUND WRITE %02x CH%02d-R%02d =%04x\n", address, ch, reg, data));
}


static void qsound_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	qsound_state *chip = (qsound_state *)param;
	UINT32 i,j;
	UINT32 offset;
	UINT32 advance;
	INT8 sample;
	struct QSOUND_CHANNEL *pC;

	memset( outputs[0], 0x00, samples * sizeof(*outputs[0]) );
	memset( outputs[1], 0x00, samples * sizeof(*outputs[1]) );
	if (! chip->sample_rom_length)
		return;

	for (i=0; i<QSOUND_CHANNELS; i++)
	{
		pC=&chip->channel[i];
		if (pC->enabled && ! pC->Muted)
		{
			for (j=0; j<samples; j++)
			{
				advance = (pC->step_ptr >> 12);
				pC->step_ptr &= 0xfff;
				pC->step_ptr += pC->freq;
				
				if (advance)
				{
					pC->address += advance;
					if (pC->freq && pC->address >= pC->end)
					{
						if (pC->loop)
						{
							// Reached the end, restart the loop
							pC->address -= pC->loop;
							
							// Make sure we don't overflow (what does the real chip do in this case?)
							if (pC->address >= pC->end)
								pC->address = pC->end - pC->loop;
							
							pC->address &= 0xffff;
						}
						else
						{
							// Reached the end of a non-looped sample
							//pC->enabled = 0;
							pC->address --;	// ensure that old ripped VGMs still work
							pC->step_ptr += 0x1000;
							break;
						}
					}
				}
				
				offset = (pC->bank | pC->address) % chip->sample_rom_length;
				sample = chip->sample_rom[offset];
				outputs[0][j] += ((sample * pC->lvol * pC->vol) >> 14);
				outputs[1][j] += ((sample * pC->rvol * pC->vol) >> 14);
			}
		}
	}
}

static void qsound_alloc_rom(void* info, UINT32 memsize)
{
	qsound_state* chip = (qsound_state *)info;
	
	if (chip->sample_rom_length == memsize)
		return;
	
	chip->sample_rom = (INT8*)realloc(chip->sample_rom, memsize);
	chip->sample_rom_length = memsize;
	memset(chip->sample_rom, 0xFF, memsize);
	
	return;
}

static void qsound_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	qsound_state* chip = (qsound_state *)info;
	
	if (offset > chip->sample_rom_length)
		return;
	if (offset + length > chip->sample_rom_length)
		length = chip->sample_rom_length - offset;
	
	memcpy(chip->sample_rom + offset, data, length);
	
	return;
}


static void qsound_set_mute_mask(void *info, UINT32 MuteMask)
{
	qsound_state* chip = (qsound_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < QSOUND_CHANNELS; CurChn ++)
		chip->channel[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
