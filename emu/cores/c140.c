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
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_C140[] =
{
	&devDef,
	NULL
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
} C140_VOICE;

typedef struct _c140_state c140_state;
struct _c140_state
{
	DEV_DATA _devData;

	UINT32 baserate;
	UINT32 sample_rate;
	UINT8 banking_type;

	UINT32 pRomSize;
	INT8 *pRom;
	UINT8 REG[0x200];

	INT16 pcmtbl[8];        //2000.06.26 CAB

	C140_VOICE voi[MAX_VOICE];
};

static void init_voice( C140_VOICE *v )
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
static INT32 find_sample(c140_state *info, INT32 adrs, INT32 bank, int voice)
{
	INT32 newadr = 0;

	static const INT16 asic219banks[4] = { 0x1f7, 0x1f1, 0x1f3, 0x1f5 };

	adrs=(bank<<16)+adrs;

	switch (info->banking_type)
	{
		case C140_TYPE_SYSTEM2:
			// System 2 banking
			newadr = ((adrs&0x200000)>>2)|(adrs&0x7ffff);
			break;

		case C140_TYPE_SYSTEM21:
			// System 21 banking.
			// similar to System 2's.
			newadr = ((adrs&0x300000)>>1)+(adrs&0x7ffff);
			break;

		case C140_TYPE_ASIC219:
			// ASIC219's banking is fairly simple
			newadr = ((info->REG[asic219banks[voice/4]]&0x3) * 0x20000) + adrs;
			break;
	}

	return (newadr);
}

static UINT8 c140_r(void *chip, UINT16 offset)
{
	c140_state *info = (c140_state *)chip;
	offset&=0x1ff;
	return info->REG[offset];
}

static void c140_w(void *chip, UINT16 offset, UINT8 data)
{
	c140_state *info = (c140_state *)chip;

	offset&=0x1ff;

	// mirror the bank registers on the 219, fixes bkrtmaq (and probably xday2 based on notes in the HLE)
	if ((offset >= 0x1f8) && (info->banking_type == C140_TYPE_ASIC219))
	{
		offset &= ~8;
	}

	info->REG[offset]=data;
	if( offset<0x180 )
	{
		C140_VOICE *v = &info->voi[offset>>4];

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
				if (info->banking_type == C140_TYPE_ASIC219)
				{
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
					v->sample_loop = (vreg->loop_msb<<8) | vreg->loop_lsb;
					v->sample_start = (vreg->start_msb<<8) | vreg->start_lsb;
					v->sample_end = (vreg->end_msb<<8) | vreg->end_lsb;
				}
			}
			else
			{
				v->key=0;
			}
		}
	}
}

static void c140_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	c140_state *info = (c140_state *)param;
	UINT32  i,j;

	INT32   rvol,lvol;
	INT32   dt;
	INT32   sdt;
	INT32   st,ed,sz;

	INT8    *pSampleData;
	INT32   frequency,delta,offset,pos;
	UINT32  cnt, voicecnt;
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

	/* get the number of voices to update */
	voicecnt = (info->banking_type == C140_TYPE_ASIC219) ? 16 : 24;

	//--- audio update
	for( i=0;i<voicecnt;i++ )
	{
		C140_VOICE *v = &info->voi[i];
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

			/* Switch on data type - compressed PCM is only for C140 */
			if ((v->mode&8) && (info->banking_type != C140_TYPE_ASIC219))
			{
				//compressed PCM (maybe correct...)
				/* Loop for enough to fill sample buffer as requested */
				for(j=0;j<samples;j++)
				{
					offset += delta;
					cnt = (offset>>16)&0x7fff;
					offset &= 0xffff;
					pos+=cnt;
					//for(;cnt>0;cnt--)
					{
						/* Check for the end of the sample */
						if(pos >= sz)
						{
							/* Check if its a looping sample, either stop or loop */
							if(v->mode&0x10)
							{
								pos = (v->sample_loop - st);
							}
							else
							{
								v->key=0;
								break;
							}
						}

						/* Read the chosen sample byte */
						dt=pSampleData[pos];

						/* decompress to 13bit range */     //2000.06.26 CAB
						sdt=dt>>3;              //signed
						if(sdt<0)   sdt = (sdt<<(dt&7)) - info->pcmtbl[dt&7];
						else        sdt = (sdt<<(dt&7)) + info->pcmtbl[dt&7];

						prevdt=lastdt;
						lastdt=sdt;
						dltdt=(lastdt - prevdt);
					}

					/* Caclulate the sample value */
					dt=((dltdt*offset)>>16)+prevdt;

					/* Write the data to the sample buffers */
					lmix[j]+=(dt*lvol)>>(5+2);
					rmix[j]+=(dt*rvol)>>(5+2);
				}
			}
			else
			{
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

						if (info->banking_type == C140_TYPE_ASIC219)
						{
							lastdt = pSampleData[pos ^ 0x01];

							// Sign + magnitude format
							if ((v->mode & 0x01) && (lastdt & 0x80))
								lastdt = -(lastdt & 0x7f);

							// Sign flip
							if (v->mode & 0x40)
								lastdt = -lastdt;
						}
						else
						{
							lastdt=pSampleData[pos];
						}

						dltdt = (lastdt - prevdt);
					}

					/* Caclulate the sample value */
					dt=((dltdt*offset)>>16)+prevdt;

					/* Write the data to the sample buffers */
					lmix[j]+=(dt*lvol)>>2;
					rmix[j]+=(dt*rvol)>>2;
				}
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

static UINT8 device_start_c140(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	c140_state *info;
	int i;
	INT32 segbase;

	info = (c140_state *)calloc(1, sizeof(c140_state));
	if (info == NULL)
		return 0xFF;
	
	info->baserate = cfg->clock / 384;	// based on MAME's notes on Namco System II
	info->sample_rate = info->baserate;
	SRATE_CUSTOM_HIGHEST(cfg->srMode, info->sample_rate, cfg->smplRate);

	info->banking_type = cfg->flags;

	info->pRomSize = 0x00;
	info->pRom = NULL;

	/* make decompress pcm table */     //2000.06.26 CAB
	segbase = 0;
	for(i = 0; i < 8; i++)
	{
		info->pcmtbl[i]=segbase;    //segment base value
		segbase += 16<<i;
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
	
	if (info->pRomSize == memsize)
		return;
	
	info->pRom = (INT8*)realloc(info->pRom, memsize);
	info->pRomSize = memsize;
	memset(info->pRom, 0xFF, memsize);
	
	return;
}

static void c140_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	c140_state *info = (c140_state *)chip;
	
	if (offset > info->pRomSize)
		return;
	if (offset + length > info->pRomSize)
		length = info->pRomSize - offset;
	
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
