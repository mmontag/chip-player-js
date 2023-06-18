// license:BSD-3-Clause
// copyright-holders:R. Belmont, superctr, Valley Bell
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
    2018.11.15  Valley Bell  split "219" from C140 code, ported channel update + MuLaw table from superctr's C352 core
*/


#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
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
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, c219_set_mute_mask},
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
	NULL,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_C219[] =
{
	&devDef,
	NULL
};


enum
{
	C219_MODE_MULAW     = 0x01, // sample is mulaw instead of linear 8-bit PCM
	C219_MODE_NOISE     = 0x04, // play noise instead of sample
	C219_MODE_L_INV     = 0x08, // left speaker phase invert
	C219_MODE_LOOP      = 0x10, // loop
	C219_MODE_INVERT    = 0x40, // invert phase
	C219_MODE_KEYON     = 0x80  // key on
};

#define MAX_VOICE 16

typedef struct
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
} C219_VREGS;

typedef struct
{
	UINT32  pos;
	UINT16  pofs;
	INT16   sample;
	INT16   last_sample;

	UINT32  sample_start;
	UINT32  sample_end;
	UINT32  sample_loop;
	UINT8   key;
	UINT8   Muted;
} C219_VOICE;

typedef struct _c219_state c219_state;
struct _c219_state
{
	DEV_DATA _devData;

	UINT32 sample_rate;

	UINT32 pRomSize;
	UINT32 pRomMask;
	UINT8 *pRom;
	UINT8 REG[0x200];

	UINT16 random;

	INT16 mulaw_table[256];

	C219_VOICE voi[MAX_VOICE];
};


/*
   find_sample: compute the actual address of a sample given it's
   address and banking registers, as well as the board type.

   I suspect in "real life" this works like the Sega MultiPCM where the banking
   is done by a small PAL or GAL external to the sound chip, which can be switched
   per-game or at least per-PCB revision as addressing range needs grow.
 */
static UINT32 find_sample(c219_state *info, UINT32 adrs, int voice)
{
	// ASIC219's banking is fairly simple
	static const UINT16 asic219banks[4] = { 0x1f7, 0x1f1, 0x1f3, 0x1f5 };
	UINT8 bank = info->REG[asic219banks[voice/4]];
	return ((bank << 17) | adrs) & info->pRomMask;
}

INLINE UINT8 c219_keyon_status_read(c219_state *info, UINT16 offset)
{
	//m_stream->update();
	C219_VOICE const *v = &info->voi[offset >> 4];

	// suzuka 8 hours and final lap games read from here, expecting bit 6 to be an in-progress sample flag.
	// four trax also expects bit 4 high for some specific channels to make engine noises to work properly
	// (sounds kinda bogus when player crashes in an object and jump spin, needs real HW verification)
	return (v->key ? 0x40 : 0x00) | (info->REG[offset] & 0x3f);
}

static UINT8 c219_r(void *chip, UINT16 offset)
{
	c219_state *info = (c219_state *)chip;
	offset &= 0x1ff;
	if (offset >= 0x1f8 && (offset & 0x001))
		offset &= ~0x008;

	// assume same as c140
	// TODO: what happens here on reading unmapped voice regs?
	if ((offset & 0xf) == 0x5 && offset < 0x100)
		return c219_keyon_status_read(info, offset);

	return info->REG[offset];
}

static void c219_w(void *chip, UINT16 offset, UINT8 data)
{
	c219_state *info = (c219_state *)chip;

	offset&=0x1ff;
	// mirror the bank registers on the 219, fixes bkrtmaq (and probably xday2 based on notes in the HLE)
	if (offset >= 0x1f8 && (offset & 0x001))
		offset &= ~0x008;

	info->REG[offset]=data;
	if (offset < 0x100)
	{
		C219_VOICE *v = &info->voi[offset>>4];

		if ((offset & 0xF) == 0x5)
		{
			if (data & C219_MODE_KEYON)
			{
				const C219_VREGS* vreg = (C219_VREGS*)&info->REG[offset & 0x1F0];

				// on the 219 asic, addresses are in words
				v->sample_loop = ((vreg->loop_msb<<8) | vreg->loop_lsb)*2;
				v->sample_start = ((vreg->start_msb<<8) | vreg->start_lsb)*2;
				v->sample_end = ((vreg->end_msb<<8) | vreg->end_lsb)*2;
				v->key=1;
				v->pos = v->sample_start;
				v->pofs = 0xFFFF;
				v->sample = 0;
				v->last_sample = 0;

				#if 0
				logerror("219: play v %d mode %02x start %x loop %x end %x\n",
					offset>>4, vreg->mode,
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

static void c219_fetch_sample(c219_state *chip, UINT32 vid)
{
	C219_VOICE* v = &chip->voi[vid];
	C219_VREGS* vreg = (C219_VREGS*)&chip->REG[vid*16];
	
	v->last_sample = v->sample;
	
	if (vreg->mode & C219_MODE_NOISE)
	{
		chip->random = (chip->random>>1) ^ ((-(chip->random&1)) & 0xfff6);
		v->sample = chip->random;
	}
	else
	{
		UINT32 addr = find_sample(chip, v->pos, vid);
		
		if (vreg->mode & C219_MODE_MULAW)
			v->sample = chip->mulaw_table[chip->pRom[addr]];
		else
			v->sample = (INT8)chip->pRom[addr] << 8;
		
		v->pos ++;
		if (v->pos == v->sample_end)
		{
			if(vreg->mode & C219_MODE_LOOP)
			{
				v->pos = v->sample_loop;
			}
			else
			{
				v->key = 0;
				v->sample = 0;
			}
		}
	}
	
	return;
}

static void c219_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	c219_state *chip = (c219_state *)param;
	UINT32 i, j;

	DEV_SMPL out[2];

	memset(outputs[0], 0, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0, samples * sizeof(DEV_SMPL));
	if (chip->pRom == NULL)
		return;

	for (i = 0; i < samples; i ++)
	{
		out[0] = out[1] = 0;

		for (j = 0; j < MAX_VOICE; j ++)
		{
			C219_VOICE* v = &chip->voi[j];
			const C219_VREGS* vreg = (C219_VREGS*)&chip->REG[j*16];

			if (v->key && ! v->Muted)
			{
				UINT32 frequency = (vreg->frequency_msb<<8) | vreg->frequency_lsb;
				INT32 newofs;
				INT32 s;

				newofs = v->pofs + frequency;
				v->pofs = newofs & 0xFFFF;
				if (newofs & 0x10000)
					c219_fetch_sample(chip, j);

				// Interpolate samples
				s = v->last_sample + (INT32)((INT64)v->pofs * (v->sample - v->last_sample) >> 16);

				if (vreg->mode & C219_MODE_INVERT)
					s = -s;
				out[0] += (((vreg->mode & C219_MODE_INVERT) ? -s : s) * vreg->volume_left);
				out[1] += (s * vreg->volume_right);
			}
		}

		outputs[0][i] += (out[0] >> 9);
		outputs[1][i] += (out[1] >> 9);
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
	
	//info->sample_rate = cfg->clock / 576;	// sample rate according to superctr
	info->sample_rate = cfg->clock / 288;	// TODO: output at 43 KHz and fix sample reading/interpolation code
	
	info->pRomSize = 0x00;
	info->pRomMask = 0x00;
	info->pRom = NULL;
	
	j = 0;
	for (i = 0; i < 128; i ++)
	{
		info->mulaw_table[i] = j << 5;
		if (i < 16)
			j += 1;
		else if (i < 24)
			j += 2;
		else if (i < 48)
			j += 4;
		else if (i < 100)
			j += 8;
		else
			j += 16;
	}
	for (i = 128; i < 256; i ++)
		info->mulaw_table[i] = (~info->mulaw_table[i - 128]) & ~0x1F;
	
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
		C219_VOICE *v = &info->voi[i];
		
		v->pofs = 0;
		v->pos = 0;
	}
	
	// init noise generator
	info->random = 0x1234;
	
	return;
}

static void c219_alloc_rom(void* chip, UINT32 memsize)
{
	c219_state *info = (c219_state *)chip;
	
	if (info->pRomSize == memsize)
		return;
	
	info->pRom = (UINT8*)realloc(info->pRom, memsize);
	info->pRomSize = memsize;
	info->pRomMask = pow2_mask(memsize);
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
