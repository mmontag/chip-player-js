// license:BSD-3-Clause
// copyright-holders:Olivier Galibert,Aaron Giles
/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../snddef.h"
#include "../EmuStructs.h"
#include "../EmuHelper.h"
#include "../EmuCores.h"
#include "rf5c68.h"


static void rf5c68_update(void *info, UINT32 samples, DEV_SMPL **outputs);

static UINT8 device_start_rf5c68_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void* device_start_rf5c68(UINT32 clock);
static void device_stop_rf5c68(void *info);
static void device_reset_rf5c68(void *info);
//void rf5c68_set_sample_end_callback(void *info, SAMPLE_END_CB callback, void* param);

static UINT8 rf5c68_r(void *info, UINT8 offset);
static void rf5c68_w(void *info, UINT8 offset, UINT8 data);

static UINT8 rf5c68_mem_r(void *info, UINT16 offset);
static void rf5c68_mem_w(void *info, UINT16 offset, UINT8 data);
static void rf5c68_write_ram(void *info, UINT32 offset, UINT32 length, const UINT8* data);

static void rf5c68_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, rf5c68_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, rf5c68_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, rf5c68_mem_w},
	{RWF_MEMORY | RWF_READ, DEVRW_A16D8, 0, rf5c68_mem_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, rf5c68_write_ram},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, rf5c68_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
DEV_DEF devDef_RF5C68_MAME =
{
	"RF5C68", "MAME", FCC_MAME,
	
	device_start_rf5c68_mame,
	device_stop_rf5c68,
	device_reset_rf5c68,
	rf5c68_update,
	
	NULL,	// SetOptionBits
	rf5c68_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};


#define NUM_CHANNELS	(8)



typedef struct _pcm_channel pcm_channel;
struct _pcm_channel
{
	UINT8		enable;
	UINT8		env;
	UINT8		pan;
	UINT8		start;
	UINT32		addr;
	UINT16		step;
	UINT16		loopst;
	UINT8		Muted;
};


typedef struct _rf5c68_state rf5c68_state;
struct _rf5c68_state
{
	DEV_DATA _devData;

	pcm_channel			chan[NUM_CHANNELS];
	UINT8				cbank;
	UINT8				wbank;
	UINT8				enable;
	UINT32				datasize;
	UINT8*				data;
	
	SAMPLE_END_CB		sample_end_cb;
	void*				sample_cb_param;
};


//-------------------------------------------------
//    RF5C68 stream update
//-------------------------------------------------

static void rf5c68_update(void *info, UINT32 samples, DEV_SMPL **outputs)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	DEV_SMPL *left = outputs[0];
	DEV_SMPL *right = outputs[1];
	UINT8 i;
	UINT32 j;

	/* start with clean buffers */
	memset(left, 0, samples * sizeof(*left));
	memset(right, 0, samples * sizeof(*right));

	/* bail if not enabled */
	if (chip->data == NULL || !chip->enable)
		return;

	/* loop over channels */
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		pcm_channel *chan = &chip->chan[i];

		/* if this channel is active, accumulate samples */
		if (chan->enable && ! chan->Muted)
		{
			int lv = (chan->pan & 0x0f) * chan->env;
			int rv = ((chan->pan >> 4) & 0x0f) * chan->env;

			/* loop over the sample buffer */
			for (j = 0; j < samples; j++)
			{
				int sample;

				/* trigger sample callback */
				if(chip->sample_end_cb)
				{
					if(((chan->addr >> 11) & 0xfff) == 0xfff)
						chip->sample_end_cb(chip->sample_cb_param,(chan->addr >> 11)/0x2000);
				}

				/* fetch the sample and handle looping */
				sample = chip->data[(chan->addr >> 11) & 0xffff];
				if (sample == 0xff)
				{
					chan->addr = chan->loopst << 11;
					sample = chip->data[(chan->addr >> 11) & 0xffff];

					/* if we loop to a loop point, we're effectively dead */
					if (sample == 0xff)
						break;
				}
				chan->addr += chan->step;

				/* add to the buffer */
				if (sample & 0x80)
				{
					sample &= 0x7f;
					left[j] += (sample * lv) >> 5;
					right[j] += (sample * rv) >> 5;
				}
				else
				{
					left[j] -= (sample * lv) >> 5;
					right[j] -= (sample * rv) >> 5;
				}
			}
		}
	}
	
#if 0	// IMO this is completely useless.
	/* now clamp and shift the result (output is only 10 bits) */
	for (j = 0; j < samples; j++)
	{
		DEV_SMPL temp;

		temp = left[j];
		if (temp > 32767) temp = 32767;
		else if (temp < -32768) temp = -32768;
		left[j] = temp & ~0x3f;

		temp = right[j];
		if (temp > 32767) temp = 32767;
		else if (temp < -32768) temp = -32768;
		right[j] = temp & ~0x3f;
	}
#endif
}


static UINT8 device_start_rf5c68_mame(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	void* chip;
	DEV_DATA* devData;
	UINT32 rate;
	
	rate = cfg->clock / 384;
	chip = device_start_rf5c68(cfg->clock);
	if (chip == NULL)
		return 0xFF;
	
	devData = (DEV_DATA*)chip;
	devData->chipInf = chip;
	INIT_DEVINF(retDevInf, devData, rate, &devDef_RF5C68_MAME);
	return 0x00;
}


//-------------------------------------------------
//    RF5C68 start
//-------------------------------------------------

void* device_start_rf5c68(UINT32 clock)
{
	rf5c68_state *chip;
	
	/* allocate memory for the chip */
	chip = (rf5c68_state *)calloc(1, sizeof(rf5c68_state));
	if (chip == NULL)
		return NULL;
	
	chip->datasize = 0x10000;
	chip->data = (UINT8*)malloc(chip->datasize);
	
	chip->sample_end_cb = NULL;
	chip->sample_cb_param = NULL;
	rf5c68_set_mute_mask(chip, 0x00);
	
	return chip;
}

static void device_stop_rf5c68(void *info)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	free(chip->data);
	free(chip);
	
	return;
}

static void device_reset_rf5c68(void *info)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	UINT8 i;
	pcm_channel* chan;
	
	// Clear the PCM memory.
	memset(chip->data, 0x00, chip->datasize);
	
	chip->enable = 0;
	chip->cbank = 0;
	chip->wbank = 0;
	
	/* clear channel registers */
	for (i = 0; i < NUM_CHANNELS; i ++)
	{
		chan = &chip->chan[i];
		chan->enable = 0;
		chan->env = 0;
		chan->pan = 0;
		chan->start = 0;
		chan->addr = 0;
		chan->step = 0;
		chan->loopst = 0;
	}
}

void rf5c68_set_sample_end_callback(void *info, SAMPLE_END_CB callback, void* param)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	
	chip->sample_end_cb = callback;
	chip->sample_cb_param = param;
	
	return;
}

//-------------------------------------------------
//    RF5C68 write register
//-------------------------------------------------

static UINT8 rf5c68_r(void *info, UINT8 offset)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	UINT8 shift;

	shift = (offset & 1) ? 11 + 8 : 11;

//	printf("%08x\n",(chip->chan[(offset & 0x0e) >> 1].addr));

	return (chip->chan[(offset & 0x0e) >> 1].addr) >> shift;
}

static void rf5c68_w(void *info, UINT8 offset, UINT8 data)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	pcm_channel *chan = &chip->chan[chip->cbank];
	UINT8 i;

	/* switch off the address */
	switch (offset)
	{
		case 0x00:	/* envelope */
			chan->env = data;
			break;

		case 0x01:	/* pan */
			chan->pan = data;
			break;

		case 0x02:	/* FDL */
			chan->step = (chan->step & 0xff00) | (data & 0x00ff);
			break;

		case 0x03:	/* FDH */
			chan->step = (chan->step & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x04:	/* LSL */
			chan->loopst = (chan->loopst & 0xff00) | (data & 0x00ff);
			break;

		case 0x05:	/* LSH */
			chan->loopst = (chan->loopst & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x06:	/* ST */
			chan->start = data;
			if (!chan->enable)
				chan->addr = chan->start << (8 + 11);
			break;

		case 0x07:	/* control reg */
			chip->enable = (data >> 7) & 1;
			if (data & 0x40)
				chip->cbank = data & 7;
			else
				chip->wbank = data & 15;
			break;

		case 0x08:	/* channel on/off reg */
			for (i = 0; i < 8; i++)
			{
				chip->chan[i].enable = (~data >> i) & 1;
				if (!chip->chan[i].enable)
					chip->chan[i].addr = chip->chan[i].start << (8 + 11);
			}
			break;
	}
}


//-------------------------------------------------
//    RF5C68 read memory
//-------------------------------------------------

static UINT8 rf5c68_mem_r(void *info, UINT16 offset)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	offset &= 0x0FFF;
	return chip->data[chip->wbank * 0x1000 | offset];
}


//-------------------------------------------------
//    RF5C68 write memory
//-------------------------------------------------

static void rf5c68_mem_w(void *info, UINT16 offset, UINT8 data)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	offset &= 0x0FFF;
	chip->data[chip->wbank * 0x1000 | offset] = data;
}

static void rf5c68_write_ram(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	
	// offset is absolute here
	if (offset >= chip->datasize)
		return;
	if (offset + length > chip->datasize)
		length = chip->datasize - offset;
	
	memcpy(chip->data + offset, data, length);
	
	return;
}


static void rf5c68_set_mute_mask(void *info, UINT32 MuteMask)
{
	rf5c68_state *chip = (rf5c68_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < NUM_CHANNELS; CurChn ++)
		chip->chan[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
