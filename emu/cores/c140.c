// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
C140.c

Simulator based on AMUSE sources.
The C140 sound chip is used by Namco System 2 and System 21
The 219 ASIC (which incorporates a modified C140) is used by Namco NA-1 and NA-2
This chip controls 24 channels (C140) or 16 (219) of PCM.
16 bytes are associated with each channel.
Channels can be 8 bit signed PCM, or 12 bit signed PCM.

TODO: What does the INT0 pin do? Normally Namco tied it to VOL0 (with VOL1 = VCC).
*/
/*
    2000.06.26  CAB     fixed compressed pcm playback
    2002.07.20  R. Belmont   added support for multiple banking types
    2006.01.08  R. Belmont   added support for NA-1/2 "219" derivative
    2018.11.16  Valley Bell  split "219" code, use correct MuLaw table (thanks superctr)
*/


#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "c140.h"

static void c140_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_c140(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_c140(void *chip);
static void device_reset_c140(void *chip);

static UINT8 c140_r(void *chip, UINT16 offset);
static void c140_w(void *chip, UINT16 offset, UINT8 data);

static void c140_alloc_rom(void* chip, UINT32 memsize);
static void c140_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);

static void c140_set_mute_mask(void *chip, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, c140_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, c140_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, c140_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, c140_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, c140_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"C140", "MAME", FCC_MAME,
	
	device_start_c140,
	device_stop_c140,
	device_reset_c140,
	c140_update,
	
	NULL,	// SetOptionBits
	c140_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_C140[] =
{
	&devDef,
	NULL
};


enum
{
	C140_MODE_MULAW     = 0x08, // sample is mulaw instead of linear 8-bit PCM
	C140_MODE_LOOP      = 0x10, // loop
	C140_MODE_KEYON     = 0x80  // key on
};

#define MAX_VOICE 24

struct voice_registers
{
	UINT8 volume_right;
	UINT8 volume_left;
	UINT8 frequency_msb;
	UINT8 frequency_lsb;
	UINT8 bank;
	UINT8 mode;
	UINT8 start_msb;
	UINT8 start_lsb;
	UINT8 end_msb;
	UINT8 end_lsb;
	UINT8 loop_msb;
	UINT8 loop_lsb;
	UINT8 reserved[4];
};

typedef struct
{
	INT32   ptoffset;
	INT32   pos;
	//--work
	INT32   lastdt;
	INT32   prevdt;
	INT32   dltdt;

	UINT32  sample_start;
	UINT32  sample_end;
	UINT32  sample_loop;
	UINT8   key;
	UINT8   Muted;
} C140_VOICE;

typedef struct _c140_state c140_state;
struct _c140_state
{
	DEV_DATA _devData;

	UINT32 clock;
	UINT32 sample_rate;
	float pbase;
	UINT8 banking_type;

	UINT32 romSize;
	UINT32 romMask;
	UINT8 *pRom;
	UINT8 REG[0x200];

	INT16 mulaw_table[256];

	C140_VOICE voi[MAX_VOICE];
};

static void init_voice( C140_VOICE *v )
{
	v->key=0;
	v->ptoffset=0;
	v->sample_start=0;
	v->sample_end=0;
	v->sample_loop=0;
}


/*
   find_sample: compute the actual address of a sample given it's
   address and banking registers, as well as the board type.

   I suspect in "real life" this works like the Sega MultiPCM where the banking
   is done by a small PAL or GAL external to the sound chip, which can be switched
   per-game or at least per-PCB revision as addressing range needs grow.
 */
static UINT32 find_sample(c140_state *info, UINT32 adrs, UINT8 bank, int voice)
{
	adrs=(bank<<16)+adrs;

	switch (info->banking_type)
	{
		case C140_TYPE_SYSTEM2:
			// System 2 banking
			return ((adrs&0x200000)>>2)|(adrs&0x7ffff);

		case C140_TYPE_SYSTEM21:
			// System 21 banking.
			// similar to System 2's.
			return ((adrs&0x300000)>>1)+(adrs&0x7ffff);

		case C140_TYPE_LINEAR:
			return adrs;
	}

	return 0;
}

INLINE UINT8 c140_keyon_status_read(c140_state *info, UINT16 offset)
{
	//m_stream->update();
	C140_VOICE const *v = &info->voi[offset >> 4];

	// suzuka 8 hours and final lap games read from here, expecting bit 6 to be an in-progress sample flag.
	// four trax also expects bit 4 high for some specific channels to make engine noises to work properly
	// (sounds kinda bogus when player crashes in an object and jump spin, needs real HW verification)
	return (v->key ? 0x40 : 0x00) | (info->REG[offset] & 0x3f);
}

static UINT8 c140_r(void *chip, UINT16 offset)
{
	c140_state *info = (c140_state *)chip;
	offset&=0x1ff;

	if ((offset & 0xf) == 0x5 && offset < 0x180)
		return c140_keyon_status_read(info, offset);

	return info->REG[offset];
}

static void c140_w(void *chip, UINT16 offset, UINT8 data)
{
	c140_state *info = (c140_state *)chip;

	offset&=0x1ff;

	info->REG[offset]=data;
	if( offset<0x180 )
	{
		C140_VOICE *v = &info->voi[offset>>4];

		if( (offset&0xf)==0x5 )
		{
			if( data&C140_MODE_KEYON )
			{
				const struct voice_registers *vreg = (struct voice_registers *) &info->REG[offset&0x1f0];
				v->key=1;
				v->ptoffset=0;
				v->pos=0;
				v->lastdt=0;
				v->prevdt=0;
				v->dltdt=0;

				v->sample_loop = (vreg->loop_msb<<8) | vreg->loop_lsb;
				v->sample_start = (vreg->start_msb<<8) | vreg->start_lsb;
				v->sample_end = (vreg->end_msb<<8) | vreg->end_lsb;
			}
			else
			{
				v->key=0;
			}
		}
	}
	else if (offset == 0x1fa)
	{
		//info->int1_callback(CLEAR_LINE);

		// timing not verified
		//unsigned div = info->REG[0x1f8] != 0 ? info->REG[0x1f8] : 256;
		//attotime interval = attotime::from_ticks(div * 2, info->baserate);
		//if (info->REG[0x1fe] & 0x01)
		//	info->int1_timer->adjust(interval);
	}
	else if (offset == 0x1fe)
	{
		if (data & 0x01)
		{
			// kyukaidk and marvlandj want the first interrupt to happen immediately
			//if (!info->int1_timer->enabled())
			//	info->int1_callback(ASSERT_LINE);
		}
		else
		{
			//info->int1_callback(CLEAR_LINE);
			//info->int1_timer->enable(false);
		}
	}
}

static void c140_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	c140_state *info = (c140_state *)param;
	UINT32  i,j;

	INT32   dt;
	INT32   sz;

	UINT32  sampleAdr;
	INT32   frequency,delta;
	UINT32  cnt;

	DEV_SMPL *lmix, *rmix;

	/* Set mixer outputs base pointers */
	lmix = outputs[0];
	rmix = outputs[1];

	/* zap the contents of the mixer buffer */
	memset(lmix, 0, samples * sizeof(DEV_SMPL));
	memset(rmix, 0, samples * sizeof(DEV_SMPL));
	if (info->pRom == NULL)
		return;

	//--- audio update
	for( i=0;i<MAX_VOICE;i++ )
	{
		C140_VOICE *v = &info->voi[i];
		const struct voice_registers *vreg = (struct voice_registers *)&info->REG[i * 16];

		if (v->key && ! v->Muted)
		{
			frequency = (vreg->frequency_msb << 8) | vreg->frequency_lsb;

			/* Abort voice if no frequency value set */
			if (frequency==0) continue;

			/* Delta =  frequency * ((8MHz/374)*2 / sample rate) */
			delta = (INT32)((float)frequency * info->pbase);

			/* calculate sample size */
			sz = v->sample_end - v->sample_start;

			/* Retrieve base pointer to the sample data */
			sampleAdr = find_sample(info, v->sample_start, vreg->bank, i);

			/* linear or compressed PCM */
			for (j = 0; j < samples; j++)
			{
				v->ptoffset += delta;
				cnt = (v->ptoffset >> 16) & 0x7fff;
				v->ptoffset &= 0xffff;
				v->pos += cnt;
				/* Check for the end of the sample */
				if (v->pos >= sz)
				{
					/* Check if its a looping sample, either stop or loop */
					if (vreg->mode & C140_MODE_LOOP)
					{
						v->pos = (v->sample_loop - v->sample_start);
					}
					else
					{
						v->key = 0;
						break;
					}
				}

				if (cnt)
				{
					v->prevdt = v->lastdt;
					if (vreg->mode & C140_MODE_MULAW)
						v->lastdt = info->mulaw_table[info->pRom[(sampleAdr + v->pos) & info->romMask]];
					else
						v->lastdt = (INT8)info->pRom[(sampleAdr + v->pos) & info->romMask] << 8;
					v->dltdt = (v->lastdt - v->prevdt);
				}

				/* Caclulate the sample value */
				dt = (INT32)(((INT64)v->dltdt * v->ptoffset) >> 16) + v->prevdt;

				/* Write the data to the sample buffers */
				lmix[j] += (dt * vreg->volume_left) >> (5 + 4);
				rmix[j] += (dt * vreg->volume_right) >> (5 + 4);
			}
		}
	}
}

static UINT8 device_start_c140(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	c140_state *info;
	int i;

	info = (c140_state *)calloc(1, sizeof(c140_state));
	if (info == NULL)
		return 0xFF;
	
	info->clock = cfg->clock;
	info->sample_rate = info->clock / 288;	// sample rate according to superctr
	SRATE_CUSTOM_HIGHEST(cfg->srMode, info->sample_rate, cfg->smplRate);
	info->pbase = (float)info->clock / 288.0f / (float)info->sample_rate;

	info->banking_type = cfg->flags;

	info->romSize = 0x00;
	info->pRom = NULL;

	for(i = 0; i < 256; i++)
	{
		UINT8 s1 = i & 7;
		UINT8 s2 = abs((INT8)i >> 3) & 0x1F;
		info->mulaw_table[i]  = (0x80 << s1) & 0xFF00;
		info->mulaw_table[i] += s2 << (s1 ? (s1+3) : 4);
		if (i & 0x80)
			info->mulaw_table[i] = -info->mulaw_table[i];
	}

	c140_set_mute_mask(info, 0x000000);
	
	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, info->sample_rate, &devDef);
	return 0x00;
}

static void device_stop_c140(void *chip)
{
	c140_state *info = (c140_state *)chip;
	
	free(info->pRom);
	free(info);
	
	return;
}

static void device_reset_c140(void *chip)
{
	c140_state *info = (c140_state *)chip;
	int i;
	
	memset(info->REG, 0, sizeof(info->REG));
	
	for(i = 0; i < MAX_VOICE; i ++)
	{
		init_voice(&info->voi[i]);
	}
	
	return;
}

static void c140_alloc_rom(void* chip, UINT32 memsize)
{
	c140_state *info = (c140_state *)chip;
	
	if (info->romSize == memsize)
		return;
	
	info->pRom = (UINT8*)realloc(info->pRom, memsize);
	info->romSize = memsize;
	info->romMask = pow2_mask(memsize);
	memset(info->pRom, 0xFF, memsize);
	
	return;
}

static void c140_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	c140_state *info = (c140_state *)chip;
	
	if (offset > info->romSize)
		return;
	if (offset + length > info->romSize)
		length = info->romSize - offset;
	
	memcpy(info->pRom + offset, data, length);
	
	return;
}


static void c140_set_mute_mask(void *chip, UINT32 MuteMask)
{
	c140_state *info = (c140_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < MAX_VOICE; CurChn ++)
		info->voi[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
