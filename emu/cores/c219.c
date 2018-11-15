// license:BSD-3-Clause
// copyright-holders:R. Belmont
/*
C219.c

Simulator based on AMUSE sources.
The C140 sound chip is used by Namco System 2 and System 21
The 219 ASIC (which incorporates a modified C140) is used by Namco NA-1 and NA-2
This chip controls 24 channels (C140) or 16 (219) of PCM.
16 bytes are associated with each channel.
Channels can be 8 bit signed PCM, or 12 bit signed PCM.

Timer behavior is not yet handled.

Unmapped registers:
    0x1f8:timer interval?   (Nx0.1 ms)
    0x1fa:irq ack? timer restart?
    0x1fe:timer switch?(0:off 1:on)

--------------

    ASIC "219" notes

    On the 219 ASIC used on NA-1 and NA-2, the high registers have the following
    meaning instead:
    0x1f7: bank for voices 0-3
    0x1f1: bank for voices 4-7
    0x1f3: bank for voices 8-11
    0x1f5: bank for voices 12-15

    Some games (bkrtmaq, xday2) write to 0x1fd for voices 12-15 instead.  Probably the bank registers
    mirror at 1f8, in which case 1ff is also 0-3, 1f9 is also 4-7, 1fb is also 8-11, and 1fd is also 12-15.

    Each bank is 0x20000 (128k), and the voice addresses on the 219 are all multiplied by 2.
    Additionally, the 219's base pitch is the same as the C352's (42667).  But these changes
    are IMO not sufficient to make this a separate file - all the other registers are
    fully compatible.

    Finally, the 219 only has 16 voices.
*/
/*
    2000.06.26  CAB     fixed compressed pcm playback
    2002.07.20  R. Belmont   added support for multiple banking types
    2006.01.08  R. Belmont   added support for NA-1/2 "219" derivative
*/


#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "c219.h"

static void c219_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_c219(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_c219(void *chip);
static void device_reset_c219(void *chip);

static UINT8 c219_r(void *chip, UINT16 offset);
static void c219_w(void *chip, UINT16 offset, UINT8 data);

static void c219_alloc_rom(void* chip, UINT32 memsize);
static void c219_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);

static void c219_set_mute_mask(void *chip, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, c219_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, c219_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, c219_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, c219_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"C219", "MAME", FCC_MAME,
	
	device_start_c219,
	device_stop_c219,
	device_reset_c219,
	c219_update,
	
	NULL,	// SetOptionBits
	c219_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_C219[] =
{
	&devDef,
	NULL
};


#define MAX_VOICE 16

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
	INT32   key;
	//--work
	INT32   lastdt;
	INT32   prevdt;
	INT32   dltdt;
	//--reg
	INT32   rvol;
	INT32   lvol;
	INT32   frequency;
	INT32   bank;
	INT32   mode;

	INT32   sample_start;
	INT32   sample_end;
	INT32   sample_loop;
	UINT8   Muted;
} C219_VOICE;

typedef struct _c219_state c219_state;
struct _c219_state
{
	DEV_DATA _devData;

	UINT32 baserate;
	UINT32 sample_rate;

	UINT32 pRomSize;
	INT8 *pRom;
	UINT8 REG[0x200];

	INT16 mulaw_table[256];

	C219_VOICE voi[MAX_VOICE];
};

static void init_voice( C219_VOICE *v )
{
	v->key=0;
	v->ptoffset=0;
	v->rvol=0;
	v->lvol=0;
	v->frequency=0;
	v->bank=0;
	v->mode=0;
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
static INT32 find_sample(c219_state *info, INT32 adrs, INT32 bank, int voice)
{
	static const INT16 asic219banks[4] = { 0x1f7, 0x1f1, 0x1f3, 0x1f5 };

	adrs=(bank<<16)|adrs;

	// ASIC219's banking is fairly simple
	return ((info->REG[asic219banks[voice/4]]&0x3) * 0x20000) + adrs;
}

static UINT8 c219_r(void *chip, UINT16 offset)
{
	c219_state *info = (c219_state *)chip;
	offset&=0x1ff;
	return info->REG[offset];
}

static void c219_w(void *chip, UINT16 offset, UINT8 data)
{
	c219_state *info = (c219_state *)chip;

	offset&=0x1ff;

	// mirror the bank registers on the 219, fixes bkrtmaq (and probably xday2 based on notes in the HLE)
	if (offset >= 0x1f8)
		offset &= ~0x008;

	info->REG[offset]=data;
	if( offset<0x180 )
	{
		C219_VOICE *v = &info->voi[offset>>4];

		if( (offset&0xf)==0x5 )
		{
			if( data&0x80 )
			{
				const struct voice_registers *vreg = (struct voice_registers *) &info->REG[offset&0x1f0];
				v->key=1;
				v->ptoffset=0;
				v->pos=0;
				v->lastdt=0;
				v->prevdt=0;
				v->dltdt=0;
				v->bank = vreg->bank;
				v->mode = data;

				// on the 219 asic, addresses are in words
				v->sample_loop = ((vreg->loop_msb<<8) | vreg->loop_lsb)*2;
				v->sample_start = ((vreg->start_msb<<8) | vreg->start_lsb)*2;
				v->sample_end = ((vreg->end_msb<<8) | vreg->end_lsb)*2;

				#if 0
				logerror("219: play v %d mode %02x start %x loop %x end %x\n",
					offset>>4, v->mode,
					find_sample(info, v->sample_start, v->bank, offset>>4),
					find_sample(info, v->sample_loop, v->bank, offset>>4),
					find_sample(info, v->sample_end, v->bank, offset>>4));
				#endif
			}
			else
			{
				v->key=0;
			}
		}
	}
}

static void c219_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	c219_state *info = (c219_state *)param;
	UINT32  i,j;

	INT32   rvol,lvol;
	INT32   dt;
	INT32   sdt;
	INT32   st,ed,sz;

	INT8    *pSampleData;
	INT32   frequency,delta,offset,pos;
	UINT32  cnt;
	INT32   lastdt,prevdt,dltdt;
	float   pbase=(float)info->baserate*2.0f / (float)info->sample_rate;

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
		C219_VOICE *v = &info->voi[i];
		const struct voice_registers *vreg = (struct voice_registers *)&info->REG[i*16];

		if( v->key && ! v->Muted)
		{
			frequency = (vreg->frequency_msb<<8) | vreg->frequency_lsb;

			/* Abort voice if no frequency value set */
			if(frequency==0) continue;

			/* Delta =  frequency * ((8MHz/374)*2 / sample rate) */
			delta=(INT32)((float)frequency * pbase);

			/* Calculate left/right channel volumes */
			lvol=(vreg->volume_left*32)/MAX_VOICE; //32ch -> 24ch
			rvol=(vreg->volume_right*32)/MAX_VOICE;

			/* Retrieve sample start/end and calculate size */
			st=v->sample_start;
			ed=v->sample_end;
			sz=ed-st;

			/* Retrieve base pointer to the sample data */
			pSampleData = info->pRom + find_sample(info, st, v->bank, i);

			/* Fetch back previous data pointers */
			offset=v->ptoffset;
			pos=v->pos;
			lastdt=v->lastdt;
			prevdt=v->prevdt;
			dltdt=v->dltdt;

			/* linear 8bit signed PCM */
			for(j=0;j<samples;j++)
			{
				offset += delta;
				cnt = (offset>>16)&0x7fff;
				offset &= 0xffff;
				pos += cnt;
				/* Check for the end of the sample */
				if(pos >= sz)
				{
					/* Check if its a looping sample, either stop or loop */
					if( v->mode&0x10 )
					{
						pos = (v->sample_loop - st);
					}
					else
					{
						v->key=0;
						break;
					}
				}

				if( cnt )
				{
					prevdt=lastdt;

					// --- 219 start ---
					lastdt = pSampleData[pos ^ 0x01];

					// Sign + magnitude format
					if ((v->mode & 0x01) && (lastdt & 0x80))
						lastdt = -(lastdt & 0x7f);

					// Sign flip
					if (v->mode & 0x40)
						lastdt = -lastdt;
					// --- 219 end ---

					dltdt = (lastdt - prevdt);
				}

				/* Caclulate the sample value */
				dt=((dltdt*offset)>>16)+prevdt;

				/* Write the data to the sample buffers */
				lmix[j]+=(dt*lvol)>>2;
				rmix[j]+=(dt*rvol)>>2;
			}

			/* Save positional data for next callback */
			v->ptoffset=offset;
			v->pos=pos;
			v->lastdt=lastdt;
			v->prevdt=prevdt;
			v->dltdt=dltdt;
		}
	}
}

static UINT8 device_start_c219(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	c219_state *info;
	int i;
	INT16 j;

	info = (c219_state *)calloc(1, sizeof(c219_state));
	if (info == NULL)
		return 0xFF;
	
	info->baserate = cfg->clock / 384;	// based on MAME's notes on Namco System II
	info->sample_rate = info->baserate;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, info->sample_rate, cfg->smplRate);

	info->pRomSize = 0x00;
	info->pRom = NULL;

	j=0;
	for(i=0;i<128;i++)
	{
		info->mulaw_table[i] = j<<5;
		if(i < 16)
			j += 1;
		else if(i < 24)
			j += 2;
		else if(i < 48)
			j += 4;
		else if(i < 100)
			j += 8;
		else
			j += 16;
	}
	for(i=128;i<256;i++)
		info->mulaw_table[i] = (~info->mulaw_table[i-128])&~0x1f;

	c219_set_mute_mask(info, 0x000000);
	
	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, info->sample_rate, &devDef);
	return 0x00;
}

static void device_stop_c219(void *chip)
{
	c219_state *info = (c219_state *)chip;
	
	free(info->pRom);
	free(info);
	
	return;
}

static void device_reset_c219(void *chip)
{
	c219_state *info = (c219_state *)chip;
	int i;
	
	memset(info->REG, 0, sizeof(info->REG));
	
	for(i = 0; i < MAX_VOICE; i ++)
	{
		init_voice(&info->voi[i]);
	}
	
	return;
}

static void c219_alloc_rom(void* chip, UINT32 memsize)
{
	c219_state *info = (c219_state *)chip;
	
	if (info->pRomSize == memsize)
		return;
	
	info->pRom = (INT8*)realloc(info->pRom, memsize);
	info->pRomSize = memsize;
	memset(info->pRom, 0xFF, memsize);
	
	return;
}

static void c219_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	c219_state *info = (c219_state *)chip;
	
	if (offset > info->pRomSize)
		return;
	if (offset + length > info->pRomSize)
		length = info->pRomSize - offset;
	
	memcpy(info->pRom + offset, data, length);
	
	return;
}


static void c219_set_mute_mask(void *chip, UINT32 MuteMask)
{
	c219_state *info = (c219_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < MAX_VOICE; CurChn ++)
		info->voi[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
