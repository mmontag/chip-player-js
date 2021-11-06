// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/*********************************************************

    Konami 054539 (TOP) PCM Sound Chip

    A lot of information comes from Amuse.
    Big thanks to them.

*********************************************************/

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
#include "k054539.h"

static void k054539_update(void *param, UINT32 samples, DEV_SMPL **outputs);
static UINT8 device_start_k054539(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_k054539(void *chip);
static void device_reset_k054539(void *chip);

static void k054539_w(void *chip, UINT16 offset, UINT8 data);
static UINT8 k054539_r(void *chip, UINT16 offset);

static void k054539_init_flags(void *chip, UINT8 _flags);
static void k054539_set_gain(void *chip, UINT8 channel, double _gain);

static void k054539_alloc_rom(void* chip, UINT32 memsize);
static void k054539_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data);

static void k054539_set_mute_mask(void *chip, UINT32 MuteMask);
static void k054539_set_log_cb(void* chip, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, k054539_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, k054539_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, k054539_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, k054539_alloc_rom},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, k054539_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"K054539", "MAME", FCC_MAME,
	
	device_start_k054539,
	device_stop_k054539,
	device_reset_k054539,
	k054539_update,
	
	NULL,	// SetOptionBits
	k054539_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	k054539_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};
const DEV_DEF* devDefList_K054539[] =
{
	&devDef,
	NULL
};


/* Registers:
   00..ff: 20 bytes/channel, 8 channels
     00..02: pitch (lsb, mid, msb)
         03: volume (0=max, 0x40=-36dB)
         04: reverb volume (idem)
     05: pan (1-f right, 10 middle, 11-1f left)
     06..07: reverb delay (0=max, current computation non-trusted)
     08..0a: loop (lsb, mid, msb)
     0c..0e: start (lsb, mid, msb) (and current position ?)

   100.1ff: effects?
     13f: pan of the analog input (1-1f)

   200..20f: 2 bytes/channel, 8 channels
     00: type (b2-3), reverse (b5)
     01: loop (b0)

   214: Key on (b0-7 = channel 0-7)
   215: Key off          ""
   225: ?
   227: Timer frequency
   228: ?
   229: ?
   22a: ?
   22b: ?
   22c: Channel active? (b0-7 = channel 0-7)
   22d: Data read/write port
   22e: ROM/RAM select (00..7f == ROM banks, 80 = Reverb RAM)
   22f: Global control:
        .......x - Enable PCM
        ......x. - Timer related?
        ...x.... - Enable ROM/RAM readback from 0x22d
        ..x..... - Timer output enable?
        x....... - Disable register RAM updates

    The chip has an optional 0x8000 byte reverb buffer.
    The reverb delay is actually an offset in this buffer.
*/

typedef struct _k054539_channel k054539_channel;
struct _k054539_channel {
	UINT32 pos;
	UINT32 pfrac;
	INT32 val;
	INT32 pval;
};

typedef struct _k054539_state k054539_state;
struct _k054539_state {
	DEV_DATA _devData;
	DEV_LOGGER logger;
	
	double voltab[256];
	double pantab[0xf];

	double gain[8];
	UINT8 posreg_latch[8][3];
	UINT8 flags;

	UINT8 regs[0x230];
	UINT8 *ram;
	UINT16 reverb_pos;

	UINT32 cur_ptr;
	UINT32 cur_limit;
	UINT8 *cur_zone;
	UINT8 *rom;
	UINT32 rom_size;
	UINT32 rom_mask;

	k054539_channel channels[8];
	UINT8 Muted[8];

	//emu_timer          *timer;
	UINT8              timer_state;
	//devcb_write_line   timer_handler;
	//k054539_cb_delegate apan_cb;

	UINT32 clock;
};

static void k054539_init_flags(void *chip, UINT8 _flags)
{
	k054539_state *info = (k054539_state *)chip;
	info->flags = _flags;
}

static void k054539_set_gain(void *chip, UINT8 channel, double _gain)
{
	k054539_state *info = (k054539_state *)chip;
	if (_gain >= 0)
		info->gain[channel] = _gain;
}
//*

static int k054539_regupdate(k054539_state *info)
{
	return !(info->regs[0x22f] & 0x80);
}

static void k054539_keyon(k054539_state *info, int channel)
{
	if(k054539_regupdate(info))
		info->regs[0x22c] |= 1 << channel;
}

static void k054539_keyoff(k054539_state *info, int channel)
{
	if(k054539_regupdate(info))
		info->regs[0x22c] &= ~(1 << channel);
}

static void k054539_update(void *param, UINT32 samples, DEV_SMPL **outputs)
{
	k054539_state *info = (k054539_state *)param;
#define VOL_CAP 1.80

	static const INT16 dpcm[16] = {
		  0 * 0x100,   1 * 0x100,   4 * 0x100,   9 * 0x100,  16 * 0x100, 25 * 0x100, 36 * 0x100, 49 * 0x100,
		-64 * 0x100, -49 * 0x100, -36 * 0x100, -25 * 0x100, -16 * 0x100, -9 * 0x100, -4 * 0x100, -1 * 0x100
	};

	INT16 *rbase = (INT16 *)info->ram;
	UINT32 sample, ch;

	if(info->rom == NULL || !(info->regs[0x22f] & 1))
	{
		memset(outputs[0], 0, samples*sizeof(*outputs[0]));
		memset(outputs[1], 0, samples*sizeof(*outputs[1]));
		return;
	}

	for(sample = 0; sample != samples; sample++) {
		double lval, rval;
		if(!(info->flags & K054539_DISABLE_REVERB))
			lval = rval = rbase[info->reverb_pos];
		else
			lval = rval = 0;
		rbase[info->reverb_pos] = 0;

		for(ch=0; ch<8; ch++)
			if(info->regs[0x22c] & (1<<ch) && ! info->Muted[ch]) {
				UINT8 *base1 = info->regs + 0x20*ch;
				UINT8 *base2 = info->regs + 0x200 + 0x2*ch;
				k054539_channel *chan = info->channels + ch;
				int delta, vol, bval, pan;
				double cur_gain, lvol, rvol, rbvol;
				UINT32 cur_pos;
				int rdelta, fdelta, pdelta;
				int cur_pfrac, cur_val, cur_pval;

				delta = base1[0x00] | (base1[0x01] << 8) | (base1[0x02] << 16);

				vol = base1[0x03];

				bval = vol + base1[0x04];
				if (bval > 255)
					bval = 255;

				pan = base1[0x05];
				// DJ Main: 81-87 right, 88 middle, 89-8f left
				if (pan >= 0x81 && pan <= 0x8f)
					pan -= 0x81;
				else if (pan >= 0x11 && pan <= 0x1f)
					pan -= 0x11;
				else
					pan = 0x18 - 0x11;

				cur_gain = info->gain[ch];

				lvol = info->voltab[vol] * info->pantab[pan] * cur_gain;
				if (lvol > VOL_CAP)
					lvol = VOL_CAP;

				rvol = info->voltab[vol] * info->pantab[0xe - pan] * cur_gain;
				if (rvol > VOL_CAP)
					rvol = VOL_CAP;

				rbvol= info->voltab[bval] * cur_gain / 2;
				if (rbvol > VOL_CAP)
					rbvol = VOL_CAP;

				rdelta = (base1[6] | (base1[7] << 8)) >> 3;
				rdelta = (rdelta + info->reverb_pos) & 0x3fff;

				cur_pos = base1[0x0c] | (base1[0x0d] << 8) | (base1[0x0e] << 16);

				if(base2[0] & 0x20) {
					delta = -delta;
					fdelta = +0x10000;
					pdelta = -1;
				} else {
					fdelta = -0x10000;
					pdelta = +1;
				}

				if(cur_pos != chan->pos) {
					chan->pos = cur_pos;
					cur_pfrac = 0;
					cur_val = 0;
					cur_pval = 0;
				} else {
					cur_pfrac = chan->pfrac;
					cur_val = chan->val;
					cur_pval = chan->pval;
				}

				switch(base2[0] & 0xc) {
				case 0x0: { // 8bit pcm
					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(info->rom[cur_pos & info->rom_mask] << 8);
						if(cur_val == (INT16)0x8000 && (base2[1] & 1)) {
							cur_pos = base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16);
							cur_val = (INT16)(info->rom[cur_pos & info->rom_mask] << 8);
						}
						if(cur_val == (INT16)0x8000) {
							k054539_keyoff(info, ch);
							cur_val = 0;
							break;
						}
					}
					break;
				}

				case 0x4: { // 16bit pcm lsb first
					pdelta <<= 1;

					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = (INT16)(info->rom[cur_pos & info->rom_mask] | info->rom[(cur_pos+1) & info->rom_mask]<<8);
						if(cur_val == (INT16)0x8000 && (base2[1] & 1)) {
							cur_pos = base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16);
							cur_val = (INT16)(info->rom[cur_pos & info->rom_mask] | info->rom[(cur_pos+1) & info->rom_mask]<<8);
						}
						if(cur_val == (INT16)0x8000) {
							k054539_keyoff(info, ch);
							cur_val = 0;
							break;
						}
					}
					break;
				}

				case 0x8: { // 4bit dpcm
					cur_pos <<= 1;
					cur_pfrac <<= 1;
					if(cur_pfrac & 0x10000) {
						cur_pfrac &= 0xffff;
						cur_pos |= 1;
					}

					cur_pfrac += delta;
					while(cur_pfrac & ~0xffff) {
						cur_pfrac += fdelta;
						cur_pos += pdelta;

						cur_pval = cur_val;
						cur_val = info->rom[(cur_pos>>1) & info->rom_mask];
						if(cur_val == 0x88 && (base2[1] & 1)) {
							cur_pos = (base1[0x08] | (base1[0x09] << 8) | (base1[0x0a] << 16)) << 1;
							cur_val = info->rom[(cur_pos>>1) & info->rom_mask];
						}
						if(cur_val == 0x88) {
							k054539_keyoff(info, ch);
							cur_val = 0;
							break;
						}
						if(cur_pos & 1)
							cur_val >>= 4;
						else
							cur_val &= 15;
						cur_val = cur_pval + dpcm[cur_val];
						if(cur_val < -32768)
							cur_val = -32768;
						else if(cur_val > 32767)
							cur_val = 32767;
					}

					cur_pfrac >>= 1;
					if(cur_pos & 1)
						cur_pfrac |= 0x8000;
					cur_pos >>= 1;
					break;
				}
				default:
					emu_logf(&info->logger, DEVLOG_DEBUG, "Unknown sample type %x for channel %d\n", base2[0] & 0xc, ch);
					info->regs[0x22c] &= ~(1<<ch);	// turn off channel to prevent spamming log messages
					break;
				}
				lval += cur_val * lvol;
				rval += cur_val * rvol;
				rbase[(rdelta + info->reverb_pos) & 0x1fff] += (INT16)(cur_val*rbvol);

				chan->pos = cur_pos;
				chan->pfrac = cur_pfrac;
				chan->pval = cur_pval;
				chan->val = cur_val;

				if(k054539_regupdate(info)) {
					base1[0x0c] = cur_pos     & 0xff;
					base1[0x0d] = cur_pos>> 8 & 0xff;
					base1[0x0e] = cur_pos>>16 & 0xff;
				}
			}
		info->reverb_pos = (info->reverb_pos + 1) & 0x1fff;
		outputs[0][sample] = (DEV_SMPL)lval;
		outputs[1][sample] = (DEV_SMPL)rval;
	}
}


//static void k054539_device_timer(void *chip)
//{
//	k054539_state *info = (k054539_state *)chip;
//	if (info->regs[0x22f] & 0x20)
//		info->timer_handler(info, info->timer_state ^= 1);
//}

static void k054539_w(void *chip, UINT16 offset, UINT8 data)
{
	k054539_state *info = (k054539_state *)chip;
	UINT8 latch;
	int ch;

	if(0) {
		int voice, reg;

		/* The K054539 has behavior like many other wavetable chips including
		   the Ensoniq 550x and Gravis GF-1: if a voice is active, writing
		   to it's current position is silently ignored.

		   Dadandaan depends on this or the vocals go wrong.
		*/
		if (offset < 8*0x20)
		{
			voice = offset / 0x20;
			reg = offset & ~0x20;

			if(info->regs[0x22c] & (1<<voice))
				if (reg >= 0xc && reg <= 0xe)
					return;
		}
	}

	latch = (info->flags & K054539_UPDATE_AT_KEYON) && (info->regs[0x22f] & 1);

	if (latch && offset < 0x100)
	{
		int offs = (offset & 0x1f) - 0xc;
		ch = offset >> 5;

		if (offs >= 0 && offs <= 2)
		{
			// latch writes to the position index registers
			info->posreg_latch[ch][offs] = data;
			return;
		}
	}

	else
		switch(offset) {
		case 0x13f: {
			int pan = data >= 0x11 && data <= 0x1f ? data - 0x11 : 0x18 - 0x11;
			//if (info->apan_cb != NULL)
			//	info->apan_cb(pantab[pan], pantab[0xe - pan]);
			break;
		}

		case 0x214:
			if (latch)
			{
				for(ch=0; ch<8; ch++)
				{
					if(data & (1<<ch))
					{
						UINT8 *posptr = &info->posreg_latch[ch][0];
						UINT8 *regptr = info->regs + (ch<<5) + 0xc;

						// update the chip at key-on
						regptr[0] = posptr[0];
						regptr[1] = posptr[1];
						regptr[2] = posptr[2];

						k054539_keyon(info, ch);
					}
				}
			}
			else
			{
				for(ch=0; ch<8; ch++)
					if(data & (1<<ch))
						k054539_keyon(info, ch);
			}
		break;

		case 0x215:
			for(ch=0; ch<8; ch++)
				if(data & (1<<ch))
					k054539_keyoff(info, ch);
		break;

		case 0x227:
		{
			//attotime period = attotime::from_hz((float)(38 + data) * (info->clock/384.0f/14400.0f)) / 2.0f;

			//info->timer->adjust(period, 0, period);

			info->timer_state = 0;
			//info->timer_handler(info->timer_state);
		}
		break;

		case 0x22d:
			if(info->regs[0x22e] == 0x80)
				info->cur_zone[info->cur_ptr] = data;
			info->cur_ptr++;
			if(info->cur_ptr == info->cur_limit)
				info->cur_ptr = 0;
		break;

		case 0x22e:
			info->cur_zone =
				data == 0x80 ? info->ram :
				&info->rom[0x20000*data];
			info->cur_limit = data == 0x80 ? 0x4000 : 0x20000;
			info->cur_ptr = 0;
		break;

		case 0x22f:
			if (!(data & 0x20)) // Disable timer output?
			{
				info->timer_state = 0;
				//info->timer_handler(info->timer_state);
			}
		break;

		default:
#if 0
			if(info->regs[offset] != data) {
				if((offset & 0xff00) == 0) {
					chanoff = offset & 0x1f;
					if(chanoff < 4 || chanoff == 5 ||
						(chanoff >=8 && chanoff <= 0xa) ||
						(chanoff >= 0xc && chanoff <= 0xe))
						break;
				}
				if(1 || ((offset >= 0x200) && (offset <= 0x210)))
					break;
				emu_logf(&info->logger, DEVLOG_TRACE, "%03x = %02x\n", offset, data);
			}
#endif
		break;
	}

	info->regs[offset] = data;
}

static void reset_zones(k054539_state *info)
{
	int data = info->regs[0x22e];
	info->cur_zone = data == 0x80 ? info->ram : &info->rom[0x20000*data];
	info->cur_limit = data == 0x80 ? 0x4000 : 0x20000;
}

static UINT8 k054539_r(void *chip, UINT16 offset)
{
	k054539_state *info = (k054539_state *)chip;
	switch(offset) {
	case 0x22d:
		if(info->regs[0x22f] & 0x10) {
			UINT8 res = info->cur_zone[info->cur_ptr];
			info->cur_ptr++;
			if(info->cur_ptr == info->cur_limit)
				info->cur_ptr = 0;
			return res;
		} else
			return 0;
	case 0x22c:
		break;
	default:
		emu_logf(&info->logger, DEVLOG_TRACE, "read %03x\n", offset);
		break;
	}
	return info->regs[offset];
}

static UINT8 device_start_k054539(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	int i;
	k054539_state *info;

	info = (k054539_state *)calloc(1, sizeof(k054539_state));
	if (info == NULL)
		return 0xFF;

	//info->timer = timer_alloc(0);

	// resolve / bind callbacks
	//info->timer_handler.resolve_safe();
	//info->apan_cb.bind_relative_to(*owner());

	for (i = 0; i < 8; i++)
		info->gain[i] = 1.0;

	info->flags = K054539_RESET_FLAGS;

	/*
	    I've tried various equations on volume control but none worked consistently.
	    The upper four channels in most MW/GX games simply need a significant boost
	    to sound right. For example, the bass and smash sound volumes in Violent Storm
	    have roughly the same values and the voices in Tokimeki Puzzledama are given
	    values smaller than those of the hihats. Needless to say the two K054539 chips
	    in Mystic Warriors are completely out of balance. Rather than forcing a
	    "one size fits all" function to the voltab the current invert exponential
	    appraoch seems most appropriate.
	*/
	// Factor the 1/4 for the number of channels in the volume (1/8 is too harsh, 1/2 gives clipping)
	// vol=0 -> no attenuation, vol=0x40 -> -36dB
	for(i=0; i<256; i++)
		info->voltab[i] = pow(10.0, (-36.0 * (double)i / (double)0x40) / 20.0) / 4.0;

	// Pan table for the left channel
	// Right channel is identical with inverted index
	// Formula is such that pan[i]**2+pan[0xe-i]**2 = 1 (constant output power)
	// and pan[0xe] = 1 (full panning)
	for(i=0; i<0xf; i++)
		info->pantab[i] = sqrt((double)i) / sqrt((double)0xe);

	info->clock = cfg->clock;

	info->flags |= K054539_UPDATE_AT_KEYON; //* make it default until proven otherwise

	info->ram = (UINT8*)malloc(0x4000);
	info->rom = NULL;
	info->rom_size = 0x00;
	info->rom_mask = 0x00;

	k054539_init_flags(info, cfg->flags);

	k054539_set_mute_mask(info, 0x00);

	info->_devData.chipInf = info;
	INIT_DEVINF(retDevInf, &info->_devData, info->clock / 384, &devDef);

	return 0x00;
}

static void device_stop_k054539(void *chip)
{
	k054539_state *info = (k054539_state *)chip;
	
	free(info->rom);	info->rom = NULL;
	free(info->ram);	info->ram = NULL;
	free(info);
	
	return;
}

static void device_reset_k054539(void *chip)
{
	k054539_state *info = (k054539_state *)chip;
	
	memset(info->regs, 0, sizeof(info->regs));
	memset(info->posreg_latch, 0, sizeof(info->posreg_latch));
	
	info->reverb_pos = 0;
	info->cur_ptr = 0;
	memset(info->ram, 0x00, 0x4000);
	
	return;
}

static void k054539_alloc_rom(void* chip, UINT32 memsize)
{
	k054539_state *info = (k054539_state *)chip;
	
	if (info->rom_size == memsize)
		return;
	
	info->rom = (UINT8*)realloc(info->rom, memsize);
	info->rom_size = memsize;
	memset(info->rom, 0xFF, memsize);
	
	info->rom_mask = pow2_mask(memsize);
	
	return;
}

static void k054539_write_rom(void *chip, UINT32 offset, UINT32 length, const UINT8* data)
{
	k054539_state *info = (k054539_state *)chip;
	
	if (offset > info->rom_size)
		return;
	if (offset + length > info->rom_size)
		length = info->rom_size - offset;
	
	memcpy(info->rom + offset, data, length);
	
	return;
}


static void k054539_set_mute_mask(void *chip, UINT32 MuteMask)
{
	k054539_state *info = (k054539_state *)chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 8; CurChn ++)
		info->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void k054539_set_log_cb(void* chip, DEVCB_LOG func, void* param)
{
	k054539_state *info = (k054539_state *)chip;
	dev_logger_set(&info->logger, info, func, param);
	return;
}
