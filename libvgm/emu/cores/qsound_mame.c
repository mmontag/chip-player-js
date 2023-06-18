// license:BSD-3-Clause
// copyright-holders:Paul Leaman, Miguel Angel Horna
/***************************************************************************

  Capcom System QSound(tm)
  ========================

  Driver by Paul Leaman and Miguel Angel Horna

  A 16 channel stereo sample player.

  QSpace position is simulated by panning the sound in the stereo space.

  Many thanks to CAB (the author of Amuse), without whom this probably would
  never have been finished.

  TODO:
  - hook up the DSP!
  - is master volume really linear?
  - understand higher bits of reg 0
  - understand reg 9
  - understand other writes to $90-$ff area

  Links:
  https://siliconpr0n.org/map/capcom/dl-1425

***************************************************************************/

#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL
#include <math.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../logging.h"
#include "qsound_mame.h"


static void qsound_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_qsound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_qsound(void *info);
static void device_reset_qsound(void *info);

static void qsound_w(void *info, UINT8 offset, UINT8 data);
static UINT8 qsound_r(void *info, UINT8 offset);
static void qsound_write_data(void *info, UINT8 address, UINT16 data);

static void qsound_alloc_rom(void* info, UINT32 memsize);
static void qsound_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data);
static void qsound_set_mute_mask(void *info, UINT32 MuteMask);
static UINT32 qsound_get_mute_mask(void *info);
static void qsound_set_log_cb(void* info, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, qsound_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, qsound_r},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D16, 0, qsound_write_data},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, qsound_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, qsound_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, qsound_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
DEV_DEF devDef_QSound_MAME =
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
	qsound_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};


#define QSOUND_CLOCK    60000000  /* default 60MHz clock */
#define QSOUND_CLOCKDIV (2*1248)  /* /2/1248 clock divider */
#define QSOUND_CHANNELS 16

typedef struct QSOUND_CHANNEL
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
} qsound_channel;

typedef struct _qsound_state qsound_state;
struct _qsound_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	qsound_channel channel[QSOUND_CHANNELS];
	
	INT8 *sample_rom;		/* Q sound sample ROM */
	UINT32 sample_rom_length;
	UINT32 sample_rom_mask;

	int pan_table[33];		/* Pan volume table */
	UINT16 data;			/* register latch data */
};

static UINT8 device_start_qsound(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	qsound_state *chip;
	int i;

	chip = (qsound_state *)calloc(1, sizeof(qsound_state));
	if (chip == NULL)
		return 0xFF;
	
	chip->sample_rom = NULL;
	chip->sample_rom_length = 0x00;
	chip->sample_rom_mask = 0x00;

	// create pan table
	for (i = 0; i < 33; i++)
		chip->pan_table[i] = (int)((256 / sqrt(32.0)) * sqrt((double)i));
	
	// init sound regs
	memset(chip->channel, 0, sizeof(chip->channel));

	qsound_set_mute_mask(chip, 0x0000);

	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, cfg->clock / QSOUND_CLOCKDIV, &devDef_QSound_MAME);

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
	UINT32 muteMask;
	int adr;

	muteMask = qsound_get_mute_mask(chip);
	// init sound regs
	memset(chip->channel, 0, sizeof(chip->channel));
	qsound_set_mute_mask(chip, muteMask);

	for (adr = 0x7f; adr >= 0; adr--)
		qsound_write_data(chip, adr, 0);
	for (adr = 0x80; adr < 0x90; adr++)
		qsound_write_data(chip, adr, 0x120);

	return;
}

static void qsound_w(void *info, UINT8 offset, UINT8 data)
{
	qsound_state *chip = (qsound_state *)info;
	switch (offset)
	{
		case 0:
			chip->data = (chip->data & 0x00ff) | (data << 8);
			break;

		case 1:
			chip->data = (chip->data & 0xff00) | data;
			break;

		case 2:
			qsound_write_data(chip, data, chip->data);
			break;

		default:
			emu_logf(&chip->logger, DEVLOG_DEBUG, "unexpected qsound write to offset %d == %02X\n", offset, data);
			break;
	}
}

static UINT8 qsound_r(void *info, UINT8 offset)
{
	/* Port ready bit (0x80 if ready) */
	return 0x80;
}


static void qsound_write_data(void *info, UINT8 address, UINT16 data)
{
	qsound_state *chip = (qsound_state *)info;
	int ch = 0, reg = 0;

	// direct sound reg
	if (address < 0x80)
	{
		ch = address >> 3;
		reg = address & 7;
	}

	// >= 0x80 is probably for the dsp?
	else if (address < 0x90)
	{
		ch = address & 0xf;
		reg = 8;
	}
	else if (address >= 0xba && address < 0xca)
	{
		ch = address - 0xba;
		reg = 9;
	}
	else
	{
		// unknown
		reg = address;
	}

	switch (reg)
	{
		case 0:
			// bank, high bits unknown
			ch = (ch + 1) & 0xf; // strange ...
			chip->channel[ch].bank = data << 16;
			break;

		case 1:
			// start/cur address
			chip->channel[ch].address = data;
			break;

		case 2:
			// frequency
			chip->channel[ch].freq = data;
#if 0
			if (data == 0)
			{
				// key off
				chip->channel[ch].enabled = 0;
			}
#endif
			break;

		case 3:
			// key on (does the value matter? it always writes 0x8000)
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
			
			chip->channel[ch].rvol = chip->pan_table[pan];
			chip->channel[ch].lvol = chip->pan_table[0x20 - pan];
			break;
		}

		case 9:
			// unknown
			break;

		default:
			//emu_logf(&chip->logger, DEVLOG_DEBUG, "write_data %02x = %04x\n", address, data);
			break;
	}
}


static void qsound_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	qsound_state *chip = (qsound_state *)param;
	UINT32 i,j;
	UINT32 offset;
	INT8 sample;
	qsound_channel *pC;

	// Clear the buffers
	memset(outputs[0], 0, samples * sizeof(*outputs[0]));
	memset(outputs[1], 0, samples * sizeof(*outputs[1]));
	if (chip->sample_rom == NULL || ! chip->sample_rom_length)
		return;

	for (j = 0; j < QSOUND_CHANNELS; j++)
	{
		pC=&chip->channel[j];
		if (pC->enabled && ! pC->Muted)
		{
			// Go through the buffer and add voice contributions
			for (i = 0; i < samples; i++)
			{
				pC->address += (pC->step_ptr >> 12);
				pC->step_ptr &= 0xfff;
				pC->step_ptr += pC->freq;

				if (pC->address >= pC->end)
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
				
				offset = pC->bank | pC->address;
				sample = chip->sample_rom[offset & chip->sample_rom_mask];
				outputs[0][i] += ((sample * pC->lvol * pC->vol) >> 14);
				outputs[1][i] += ((sample * pC->rvol * pC->vol) >> 14);
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
	chip->sample_rom_mask = pow2_mask(memsize);
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

static UINT32 qsound_get_mute_mask(void *info)
{
	qsound_state* chip = (qsound_state *)info;
	UINT32 muteMask;
	UINT8 CurChn;
	
	muteMask = 0x0000;
	for (CurChn = 0; CurChn < QSOUND_CHANNELS; CurChn ++)
		muteMask |= (chip->channel[CurChn].Muted << CurChn);
	
	return muteMask;
}

static void qsound_set_log_cb(void* info, DEVCB_LOG func, void* param)
{
	qsound_state* chip = (qsound_state *)info;
	dev_logger_set(&chip->logger, chip, func, param);
	return;
}
