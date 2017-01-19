/*
    c352.c - Namco C352 custom PCM chip emulation
    v2.0
    
    Rewritten by superctr
    
    Original code by R. Belmont
    Additional code by cync and the hoot development team

    Thanks to Cap of VivaNonno for info and The_Author for preliminary reverse-engineering

    Chip specs:
    32 voices
    Supports 8-bit linear and 8-bit muLaw samples
    Output: digital, 16 bit, 4 channels
    Output sample rate is the input clock / (288 * 2).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "c352.h"

static void c352_update(void *chip, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_c352(const C352_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_c352(void *chip);
static void device_reset_c352(void *chip);

static UINT16 c352_r(void *chip, UINT16 offset);
static void c352_w(void *chip, UINT16 offset, UINT16 data);

static void c352_alloc_rom(void* chip, UINT32 memsize);
static void c352_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);

static void c352_set_mute_mask(void *chip, UINT32 MuteMask);
static void c352_set_options(void *chip, UINT32 Flags);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D16, 0, c352_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D16, 0, c352_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, c352_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, c352_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"C352", "MAME", FCC_MAME,
	
	(DEVFUNC_START)device_start_c352,
	device_stop_c352,
	device_reset_c352,
	c352_update,
	
	c352_set_options,	// SetOptionBits
	c352_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_C352[] =
{
	&devDef,
	NULL
};


#define C352_VOICES 32
enum {
	C352_FLG_BUSY       = 0x8000,   // channel is busy
	C352_FLG_KEYON      = 0x4000,   // Keyon
	C352_FLG_KEYOFF     = 0x2000,   // Keyoff
	C352_FLG_LOOPTRG    = 0x1000,   // Loop Trigger
	C352_FLG_LOOPHIST   = 0x0800,   // Loop History
	C352_FLG_FM         = 0x0400,   // Frequency Modulation
	C352_FLG_PHASERL    = 0x0200,   // Rear Left invert phase 180 degrees
	C352_FLG_PHASEFL    = 0x0100,   // Front Left invert phase 180 degrees
	C352_FLG_PHASEFR    = 0x0080,   // invert phase 180 degrees (e.g. flip sign of sample)
	C352_FLG_LDIR       = 0x0040,   // loop direction
	C352_FLG_LINK       = 0x0020,   // "long-format" sample (can't loop, not sure what else it means)
	C352_FLG_NOISE      = 0x0010,   // play noise instead of sample
	C352_FLG_MULAW      = 0x0008,   // sample is mulaw instead of linear 8-bit PCM
	C352_FLG_FILTER     = 0x0004,   // don't apply filter
	C352_FLG_REVLOOP    = 0x0003,   // loop backwards
	C352_FLG_LOOP       = 0x0002,   // loop forward
	C352_FLG_REVERSE    = 0x0001    // play sample backwards
};

typedef struct {

	UINT32 pos;
	UINT32 counter;
	
	INT16 sample;
	INT16 last_sample;

	UINT16 vol_f;
	UINT16 vol_r;
	UINT16 freq;
	UINT16 flags;
	
	UINT16 wave_bank;
	UINT16 wave_start;
	UINT16 wave_end;
	UINT16 wave_loop;
	
	UINT8 mute;

} C352_Voice;

typedef struct {

	void* chipInf;

	UINT32 rate;
	UINT8 muteRear;     // flag from VGM header
	UINT8 optMuteRear;  // option

	C352_Voice v[C352_VOICES];

	UINT16 control1; // unknown purpose for both
	UINT16 control2;

	UINT8* wave;
	UINT32 wavesize;
	UINT32 wave_mask;

	UINT16 random;
	
	INT16 mulaw[256];

} C352;


static void C352_generate_mulaw(C352 *c)
{
	int i;
	double x_max = 32752.0;
	double y_max = 127.0;
	double u = 10.0;

	// generate mulaw table for mulaw format samples
	for (i = 0; i < 256; i++)
	{
		double y = (double) (i & 0x7f);
		double x = (exp (y / y_max * log (1.0 + u)) - 1.0) * x_max / u;

		if (i & 0x80)
			x = -x;
		c->mulaw[i] = (short)x;
	}
}

static void C352_fetch_sample(C352 *c, int i)
{
	C352_Voice *v = &c->v[i];
	v->last_sample = v->sample;
	
	if(v->flags & C352_FLG_NOISE)
	{
		c->random = (c->random>>1) ^ (-(c->random&1) & 0xfff6);
		v->sample = (c->random&4) ? 0xc000 : 0x3fff;
		
		v->last_sample = v->sample; // No interpolation for noise samples
	}
	else
	{
		INT8 s;
		UINT16 pos;

		s = (INT8)c->wave[v->pos&0xffffff];

		if(v->flags & C352_FLG_MULAW)
			v->sample = c->mulaw[(UINT8)s];
		else
			v->sample = s<<8;
		
		pos = v->pos&0xffff;
		
		if((v->flags & C352_FLG_LOOP) && v->flags & C352_FLG_REVERSE)
		{
			// backwards>forwards
			if((v->flags & C352_FLG_LDIR) && pos == v->wave_loop)
				v->flags &= ~C352_FLG_LDIR;
			// forwards>backwards
			else if(!(v->flags & C352_FLG_LDIR) && pos == v->wave_end)
				v->flags |= C352_FLG_LDIR;
			
			v->pos += (v->flags&C352_FLG_LDIR) ? -1 : 1;
		}
		else if(pos == v->wave_end)
		{
			if((v->flags & C352_FLG_LINK) && (v->flags & C352_FLG_LOOP))
			{
				v->pos = (v->wave_start<<16) | v->wave_loop;
				v->flags |= C352_FLG_LOOPHIST;
			}
			else if(v->flags & C352_FLG_LOOP)
			{
				v->pos = (v->pos&0xff0000) | v->wave_loop;
				v->flags |= C352_FLG_LOOPHIST;
			}
			else
			{
				v->flags |= C352_FLG_KEYOFF;
				v->flags &= ~C352_FLG_BUSY;
				v->sample=0;
				v->last_sample=0;
			}
		}
		else
		{
			v->pos += (v->flags&C352_FLG_REVERSE) ? -1 : 1;
		}
	}
}

static UINT16 C352_update_voice(C352 *c, int i)
{
	C352_Voice *v = &c->v[i];
	INT32 temp;

	if((v->flags & C352_FLG_BUSY) == 0)
		return 0;

	v->counter += v->freq;
	
	if(v->counter > 0x10000)
	{
		v->counter &= 0xffff;
		C352_fetch_sample(c,i);
	}
	
	temp = v->sample;
	
	if((v->flags & C352_FLG_FILTER) == 0)
		temp = v->last_sample + (v->counter*(v->sample-v->last_sample)>>16);

	return temp;
}

static void c352_update(void *chip, UINT32 samples, DEV_SMPL **outputs)
{
	C352 *c = (C352 *)chip;
	UINT32 i, j;
	INT16 s;
	memset(outputs[0], 0x00, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0x00, samples * sizeof(DEV_SMPL));
	
	for(i=0;i<samples;i++)
	{
		for(j=0;j<C352_VOICES;j++)
		{
			s = C352_update_voice(c,j);
			if(!c->v[j].mute)
			{
				// Left
				outputs[0][i] += (c->v[j].flags & C352_FLG_PHASEFL) ? (-s * (c->v[j].vol_f>>8)  )>>8
				                                                  : ( s * (c->v[j].vol_f>>8)  )>>8;
				if (!c->muteRear && !c->optMuteRear)
					outputs[0][i] += (c->v[j].flags & C352_FLG_PHASERL) ? (-s * (c->v[j].vol_r>>8)  )>>8
				                                                      : ( s * (c->v[j].vol_r>>8)  )>>8;
				
				// Right
				outputs[1][i] += (c->v[j].flags & C352_FLG_PHASEFR) ? (-s * (c->v[j].vol_f&0xff))>>8
				                                                  : ( s * (c->v[j].vol_f&0xff))>>8;
				if (!c->muteRear && !c->optMuteRear)
					outputs[1][i] += ( s * (c->v[j].vol_r&0xff))>>8;
			}
		}
		
	}
}

static UINT8 device_start_c352(const C352_CFG* cfg, DEV_INFO* retDevInf)
{
	C352 *c;
	UINT16 clkdiv;

	c = (C352 *)calloc(1, sizeof(C352));
	if (c == NULL)
		return 0xFF;

	c->wave = NULL;
	c->wavesize = 0x00;

	clkdiv = cfg->clk_divider ? cfg->clk_divider : 288;
	c->rate = CHPCLK_CLOCK(cfg->_genCfg.clock)/clkdiv;
	c->muteRear = CHPCLK_FLAG(cfg->_genCfg.clock);

	//memset(c->v,0,sizeof(C352_Voice)*C352_VOICES);

	c->control1 = 0;
	c->control2 = 0;
	c->random = 0x1234;

	C352_generate_mulaw(c);

	c352_set_mute_mask(c, 0x00000000);

	c->chipInf = c;
	INIT_DEVINF(retDevInf, (DEV_DATA*)c, c->rate, &devDef);
	return 0x00;
}

static void device_stop_c352(void *chip)
{
	C352 *c = (C352 *)chip;
	
	free(c->wave);
	free(c);
	
	return;
}

static void device_reset_c352(void *chip)
{
	C352 *c = (C352 *)chip;
	
	// TODO: save/restore muting flags
	memset(c->v,0,sizeof(C352_Voice)*C352_VOICES);
	
	return;
}

static UINT16 C352RegMap[8] = {
	offsetof(C352_Voice,vol_f) / sizeof(UINT16),
	offsetof(C352_Voice,vol_r) / sizeof(UINT16),
	offsetof(C352_Voice,freq) / sizeof(UINT16),
	offsetof(C352_Voice,flags) / sizeof(UINT16),
	offsetof(C352_Voice,wave_bank) / sizeof(UINT16),
	offsetof(C352_Voice,wave_start) / sizeof(UINT16),
	offsetof(C352_Voice,wave_end) / sizeof(UINT16),
	offsetof(C352_Voice,wave_loop) / sizeof(UINT16),
};

static UINT16 c352_r(void *chip, UINT16 offset)
{
	C352 *c = (C352 *)chip;

	if(offset < 0x100)
		return *((UINT16*)&c->v[offset/8]+C352RegMap[offset%8]);
	else
		return 0;
}

static void c352_w(void *chip, UINT16 offset, UINT16 data)
{
	C352 *c = (C352 *)chip;
	
	int i;
	
	if(offset < 0x100) // Channel registers, see map above.
		*((UINT16*)&c->v[offset/8]+C352RegMap[offset%8]) = data;
	else if(offset == 0x200) // Unknown purpose.
		c->control1 = data;
	else if(offset == 0x201)
		c->control2 = data;
	else if(offset == 0x202) // execute keyons/keyoffs
	{
		for(i=0;i<C352_VOICES;i++)
		{
			if((c->v[i].flags & C352_FLG_KEYON))
			{
				c->v[i].pos = (c->v[i].wave_bank<<16) | c->v[i].wave_start;

				c->v[i].sample = 0;
				c->v[i].last_sample = 0;
				c->v[i].counter = 0x10000; // Immediate update

				c->v[i].flags |= C352_FLG_BUSY;
				c->v[i].flags &= ~(C352_FLG_KEYON|C352_FLG_LOOPHIST);
			}
			else if(c->v[i].flags & C352_FLG_KEYOFF)
			{
				c->v[i].sample=0;
				c->v[i].last_sample=0;
				c->v[i].flags &= ~(C352_FLG_BUSY|C352_FLG_KEYOFF);
			}
		}
	}
}


static void c352_alloc_rom(void* chip, UINT32 memsize)
{
	C352 *c = (C352 *)chip;
	
	if (c->wavesize == memsize)
		return;
	
	c->wave = (UINT8*)realloc(c->wave, memsize);
	c->wavesize = memsize;
	memset(c->wave, 0xFF, memsize);
	
	return;
}

static void c352_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	C352 *c = (C352 *)chip;
	
	if (offset > c->wavesize)
		return;
	if (offset + length > c->wavesize)
		length = c->wavesize - offset;
	
	memcpy(c->wave + offset, data, length);
	
	return;
}

static void c352_set_mute_mask(void *chip, UINT32 MuteMask)
{
	C352 *c = (C352 *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < C352_VOICES; CurChn ++)
		c->v[CurChn].mute = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void c352_set_options(void *chip, UINT32 Flags)
{
	C352 *c = (C352 *)chip;
	
	c->optMuteRear = (Flags & 0x01) >> 0;
	
	return;
}
