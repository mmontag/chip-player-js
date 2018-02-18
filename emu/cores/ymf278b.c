// C-port of YMF278B.cc from openMSX
// license:GPL-2.0
/*

   YMF278B  FM + Wave table Synthesizer (OPL4)

   Timer and PCM YMF278B.  The FM is shared with the ymf262.

   This chip roughly splits the difference between the Sega 315-5560 MultiPCM
   (Multi32, Model 1/2) and YMF 292-F SCSP (later Model 2, STV, Saturn, Model 3).

   Features as listed in LSI-4MF2782 data sheet:
    FM Synthesis (same as YMF262)
     1. Sound generation mode
         Two-operater mode
          Generates eighteen voices or fifteen voices plus five rhythm sounds simultaneously
         Four-operator mode
          Generates six voices in four-operator mode plus six voices in two-operator mode simultaneously,
          or generates six voices in four-operator mode plus three voices in two-operator mode plus five
          rhythm sounds simultaneously
     2. Eight selectable waveforms
     3. Stereo output
    Wave Table Synthesis
     1. Generates twenty-four voices simultaneously
     2. 44.1kHz sampling rate for output sound data
     3. Selectable from 8-bit, 12-bit and 16-bit word lengths for wave data
     4. Stereo output (16-stage panpot for each voice)
    Wave Data
     1. Accepts 32M bit external memory at maximum
     2. Up to 512 wave tables
     3. External ROM or SRAM can be connected. With SRAM connected, the CPU can download wave data
     4. Outputs chip select signals for 1Mbit, 4Mbit, 8Mbit or 16Mbit memory
     5. Can be directly connected to the Yamaha YRW801 (Wave data ROM)
        Features of YRW801 as listed in LSI 4RW801A2
          Built-in wave data of tones which comply with GM system Level 1
           Melody tone ....... 128 tones
           Percussion tone ...  47 tones
          16Mbit capacity (2,097,152word x 8)
*/

// Based on ymf278b.c written by R. Belmont and O. Galibert

// This class doesn't model a full YMF278b chip. Instead it only models the
// wave part. The FM part in modeled in YMF262 (it's almost 100% compatible,
// the small differences are handled in YMF262). The status register and
// interaction with the FM registers (e.g. the NEW2 bit) is currently handled
// in the MSXMoonSound class.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../SoundDevs.h"
#include "../SoundEmu.h"
#include "../EmuHelper.h"
#include "ymf278b.h"


#define LINKDEV_OPL3	0x00

typedef struct _YMF278BChip YMF278BChip;

static void ymf278b_pcm_update(void *info, UINT32 samples, DEV_SMPL** outputs);
static void refresh_opl3_volume(YMF278BChip* chip);
static void init_opl3_devinfo(DEV_INFO* devInf, const DEV_GEN_CFG* baseCfg);
static UINT8 device_start_ymf278b(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_ymf278b(void *info);
static void device_reset_ymf278b(void *info);
static UINT8 device_ymf278b_link_opl3(void* param, UINT8 devID, const DEV_INFO* defInfOPL3);

static UINT8 ymf278b_r(void *info, UINT8 offset);
static void ymf278b_w(void *info, UINT8 offset, UINT8 data);
static void ymf278b_alloc_rom(void* info, UINT32 memsize);
static void ymf278b_alloc_ram(void* info, UINT32 memsize);
static void ymf278b_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data);
static void ymf278b_write_ram(void *info, UINT32 offset, UINT32 length, const UINT8* data);

static void ymf278b_set_mute_mask(void *info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ymf278b_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ymf278b_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x524F, ymf278b_write_rom},	// 0x524F = 'RO' for ROM
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x524F, ymf278b_alloc_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x5241, ymf278b_write_ram},	// 0x5241 = 'RA' for RAM
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x5241, ymf278b_alloc_ram},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"YMF278B", "openMSX", FCC_OMSX,
	
	device_start_ymf278b,
	device_stop_ymf278b,
	device_reset_ymf278b,
	ymf278b_pcm_update,
	
	NULL,	// SetOptionBits
	ymf278b_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	device_ymf278b_link_opl3,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_YMF278B[] =
{
	&devDef,
	NULL
};


typedef struct
{
	UINT32 startaddr;
	UINT16 loopaddr;
	UINT16 endaddr;
	UINT32 step;	// fixed-point frequency step
					// invariant: step == calcStep(OCT, FN)
	UINT32 stepptr;	// fixed-point pointer into the sample
	UINT16 pos;
	INT16 sample1, sample2;

	INT32 env_vol;

	INT32 lfo_cnt;
	INT32 lfo_step;
	INT32 lfo_max;

	INT32 DL;
	INT16 wave;		// wavetable number
	INT16 FN;		// f-number         TODO store 'FN | 1024'?
	UINT8 OCT;		// octave [0..15]   TODO store sign-extended?
	UINT8 PRVB;		// pseudo-reverb
	UINT8 LD;		// level direct
	UINT8 TLdest;	// destination total level
	UINT8 TL;		// total level
	UINT8 pan;		// panpot
	UINT8 lfo;		// LFO
	UINT8 vib;		// vibrato
	UINT8 AM;		// AM level
	UINT8 AR;
	UINT8 D1R;
	UINT8 D2R;
	UINT8 RC;		// rate correction
	UINT8 RR;

	UINT8 bits;		// width of the samples
	UINT8 active;	// slot keyed on

	UINT8 state;
	UINT8 lfo_active;

	UINT8 Muted;
} YMF278BSlot;

typedef struct
{
	void* chip;
	DEVFUNC_WRITE_A8D8 write;
	DEVFUNC_CTRL reset;
	DEVFUNC_WRITE_VOL_LR setVol;
} OPL3FM;
struct _YMF278BChip
{
	DEV_DATA _devData;

	YMF278BSlot slots[24];

	/** Global envelope generator counter. */
	UINT32 eg_cnt;
	UINT32 tl_int_cnt;
	UINT8 tl_int_step;	// modulo 3 counter

	INT32 memadr;

	INT32 fm_l, fm_r;
	INT32 pcm_l, pcm_r;

	UINT32 ROMSize;
	UINT8 *rom;
	UINT32 RAMSize;
	UINT8 *ram;
	UINT32 clock;

	/** Precalculated attenuation values with some margin for
	  * envelope and pan levels.
	  */
	INT32 volume[0x80];

	UINT8 regs[0x100];

	UINT8 exp;

	UINT8 port_AB, port_C, lastport;

	UINT8 last_fm_data;
	OPL3FM fm;
};

INLINE UINT8* ymf278b_getMemPtr(YMF278BChip* chip, UINT32 address);
INLINE UINT8 ymf278b_readMem(YMF278BChip* chip, UINT32 address);
INLINE void ymf278b_writeMem(YMF278BChip* chip, UINT32 address, UINT8 value);


#define EG_SH				16 // 16.16 fixed point (EG timing)
#define EG_TIMER_OVERFLOW	(1 << EG_SH)

// envelope output entries
#define ENV_BITS			10
#define ENV_LEN				(1 << ENV_BITS)
#define ENV_STEP			(128.0f / ENV_LEN)
#define MAX_ATT_INDEX		((1 << (ENV_BITS - 1)) - 1)	// 511
#define MIN_ATT_INDEX		0

// Envelope Generator phases
#define EG_ATT	4
#define EG_DEC	3
#define EG_SUS	2
#define EG_REL	1
#define EG_OFF	0

#define EG_REV	5 // pseudo reverb
#define EG_DMP	6 // damp

// Pan values, units are -3dB, i.e. 8.
static const INT32 pan_left[16]  = {
	0, 8, 16, 24, 32, 40, 48, 128, 128,   0,  0,  0,  0,  0,  0, 0
};
static const INT32 pan_right[16] = {
	0, 0,  0,  0,  0,  0,  0,   0, 128, 128, 48, 40, 32, 24, 16, 8
};

// Mixing levels, units are -3dB
static const INT32 mix_level[8] = {
	0, 8, 16, 24, 32, 40, 48, 128
};

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
#define SC(db) (UINT32)(db * (2.0 / ENV_STEP))
static const UINT32 dl_tab[16] = {
 SC( 0), SC( 1), SC( 2), SC(3 ), SC(4 ), SC(5 ), SC(6 ), SC( 7),
 SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
};
#undef SC

#define RATE_STEPS	8
static const UINT8 eg_inc[15 * RATE_STEPS] = {
//cycle:0  1   2  3   4  5   6  7
	0, 1,  0, 1,  0, 1,  0, 1, //  0  rates 00..12 0 (increment by 0 or 1)
	0, 1,  0, 1,  1, 1,  0, 1, //  1  rates 00..12 1
	0, 1,  1, 1,  0, 1,  1, 1, //  2  rates 00..12 2
	0, 1,  1, 1,  1, 1,  1, 1, //  3  rates 00..12 3

	1, 1,  1, 1,  1, 1,  1, 1, //  4  rate 13 0 (increment by 1)
	1, 1,  1, 2,  1, 1,  1, 2, //  5  rate 13 1
	1, 2,  1, 2,  1, 2,  1, 2, //  6  rate 13 2
	1, 2,  2, 2,  1, 2,  2, 2, //  7  rate 13 3

	2, 2,  2, 2,  2, 2,  2, 2, //  8  rate 14 0 (increment by 2)
	2, 2,  2, 4,  2, 2,  2, 4, //  9  rate 14 1
	2, 4,  2, 4,  2, 4,  2, 4, // 10  rate 14 2
	2, 4,  4, 4,  2, 4,  4, 4, // 11  rate 14 3

	4, 4,  4, 4,  4, 4,  4, 4, // 12  rates 15 0, 15 1, 15 2, 15 3 for decay
	8, 8,  8, 8,  8, 8,  8, 8, // 13  rates 15 0, 15 1, 15 2, 15 3 for attack (zero time)
	0, 0,  0, 0,  0, 0,  0, 0, // 14  infinity rates for attack and decay(s)
};

#define O(a) ((a) * RATE_STEPS)
static const UINT8 eg_rate_select[64] = {
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 4),O( 5),O( 6),O( 7),
	O( 8),O( 9),O(10),O(11),
	O(12),O(12),O(12),O(12),
};
#undef O

// rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
// shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
// mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
#define O(a) (a)
static const UINT8 eg_rate_shift[64] = {
	O(12),O(12),O(12),O(12),
	O(11),O(11),O(11),O(11),
	O(10),O(10),O(10),O(10),
	O( 9),O( 9),O( 9),O( 9),
	O( 8),O( 8),O( 8),O( 8),
	O( 7),O( 7),O( 7),O( 7),
	O( 6),O( 6),O( 6),O( 6),
	O( 5),O( 5),O( 5),O( 5),
	O( 4),O( 4),O( 4),O( 4),
	O( 3),O( 3),O( 3),O( 3),
	O( 2),O( 2),O( 2),O( 2),
	O( 1),O( 1),O( 1),O( 1),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
};
#undef O


// number of steps to take in quarter of lfo frequency
// TODO check if frequency matches real chip
#define O(a) (INT32)((EG_TIMER_OVERFLOW / a) / 6)
static const INT32 lfo_period[8] = {
	O(0.168f), O(2.019f), O(3.196f), O(4.206f),
	O(5.215f), O(5.888f), O(6.224f), O(7.066f)
};
#undef O


#define O(a) (INT32)((a) * 65536)
static const INT32 vib_depth[8] = {
	O( 0.0f  ), O( 3.378f), O( 5.065f), O( 6.750f),
	O(10.114f), O(20.170f), O(40.106f), O(79.307f)
};
#undef O


#define SC(db) (INT32)((db) * (2.0f / ENV_STEP))
static const INT32 am_depth[8] = {
	SC(0.0f  ), SC(1.781f), SC(2.906f), SC( 3.656f),
	SC(4.406f), SC(5.906f), SC(7.406f), SC(11.91f )
};
#undef SC


// Sign extend a 4-bit value to int (32-bit)
// require: x in range [0..15]
INLINE int sign_extend_4(UINT8 x)
{
	return ((int)x ^ 8) - 8;
}

// Params: oct in [0 ..   15]
//         fn  in [0 .. 1023]
// We want to interpret oct as a signed 4-bit number and calculate
//    ((fn | 1024) + vib) << (5 + sign_extend_4(oct))
// Though in this formula the shift can go over a negative distance (in that
// case we should shift in the other direction).
INLINE UINT32 calcStep(UINT8 oct, UINT32 fn, int vib)
{
	UINT32 t;
	oct ^= 8; // [0..15] -> [8..15][0..7] == sign_extend_4(x) + 8
	t = (fn + 1024 + vib) << oct; // use '+' iso '|' (generates slightly better code)
	return t >> 3; // was shifted 3 positions too far
}

static void ymf278b_slot_reset(YMF278BSlot* slot)
{
	slot->wave = slot->FN = slot->OCT = slot->PRVB = slot->LD = slot->TLdest = slot->TL = slot->pan =
		slot->lfo = slot->vib = slot->AM = 0;
	slot->AR = slot->D1R = slot->DL = slot->D2R = slot->RC = slot->RR = 0;
	slot->stepptr = 0;
	slot->step = calcStep(slot->OCT, slot->FN, 0);
	slot->bits = slot->startaddr = slot->loopaddr = slot->endaddr = 0;
	slot->env_vol = MAX_ATT_INDEX;

	slot->lfo_active = 0;
	slot->lfo_cnt = slot->lfo_step = 0;
	slot->lfo_max = lfo_period[0];

	slot->state = EG_OFF;
	slot->active = 0;

	// not strictly needed, but avoid UMR on savestate
	slot->pos = slot->sample1 = slot->sample2 = 0;
}

INLINE int ymf278b_slot_compute_rate(YMF278BSlot* slot, int val)
{
	int res;
	
	if (val == 0)
		return 0;
	else if (val == 15)
		return 63;
	
	if (slot->RC != 15)
	{
		// TODO it may be faster to store 'OCT' sign extended
		int oct = sign_extend_4(slot->OCT);
		res = (oct + slot->RC) * 2 + (slot->FN & 0x200 ? 1 : 0) + val * 4;
	}
	else
	{
		res = val * 4;
	}
	
	if (res < 0)
		res = 0;
	else if (res > 63)
		res = 63;
	
	return res;
}

INLINE int ymf278b_slot_compute_vib(YMF278BSlot* slot)
{
	return (((slot->lfo_step << 8) / slot->lfo_max) * vib_depth[slot->vib]) >> 24;
}


INLINE int ymf278b_slot_compute_am(YMF278BSlot* slot)
{
	if (slot->lfo_active && slot->AM)
		return (((slot->lfo_step << 8) / slot->lfo_max) * am_depth[slot->AM]) >> 12;
	else
		return 0;
}

INLINE void ymf278b_slot_set_lfo(YMF278BSlot* slot, UINT8 newlfo)
{
	slot->lfo_step = (((slot->lfo_step << 8) / slot->lfo_max) * (INT32)newlfo) >> 8;
	slot->lfo_cnt  = (((slot->lfo_cnt  << 8) / slot->lfo_max) * (INT32)newlfo) >> 8;

	slot->lfo = newlfo;
	slot->lfo_max = lfo_period[slot->lfo];
}


static void ymf278b_advance(YMF278BChip* chip)
{
	YMF278BSlot* op;
	int i;
	UINT8 rate;
	UINT8 shift;
	UINT8 select;
	
	chip->tl_int_cnt ++;
	if (chip->tl_int_cnt >= 9)
	{
		chip->tl_int_cnt -= 9;
		chip->tl_int_step ++;
		if (chip->tl_int_step >= 3)
			chip->tl_int_step -= 3;
	}

	chip->eg_cnt ++;
	for (i = 0; i < 24; i ++)
	{
		op = &chip->slots[i];

		if (! chip->tl_int_cnt)
		{
			if (op->TL < op->TLdest)
			{
				// one step every 27 samples
				if (chip->tl_int_step == 0)
					op->TL ++;
			}
			else if (op->TL > op->TLdest)
			{
				// one step every 13.5 samples
				if (chip->tl_int_step > 0)
					op->TL --;
			}
		}

		if (op->lfo_active)
		{
			op->lfo_cnt ++;
			if (op->lfo_cnt < op->lfo_max)
			{
				op->lfo_step ++;
			}
			else if (op->lfo_cnt < (op->lfo_max * 3))
			{
				op->lfo_step --;
			}
			else
			{
				op->lfo_step ++;
				if (op->lfo_cnt == (op->lfo_max * 4))
					op->lfo_cnt = 0;
			}
		}

		// Envelope Generator
		switch(op->state)
		{
		case EG_ATT:	// attack phase
			rate = ymf278b_slot_compute_rate(op, op->AR);
			if (rate < 4)
				break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += (~op->env_vol * eg_inc[select + ((chip->eg_cnt >> shift) & 7)]) >> 3;
				if (op->env_vol <= MIN_ATT_INDEX)
				{
					op->env_vol = MIN_ATT_INDEX;
					if (op->DL)
						op->state = EG_DEC;
					else
						op->state = EG_SUS;
				}
			}
			break;
		case EG_DEC:	// decay phase
			rate = ymf278b_slot_compute_rate(op, op->D1R);
			if (rate < 4)
				break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];

				if ((op->env_vol > dl_tab[6]) && op->PRVB)
					op->state = EG_REV;
				else
				{
					if (op->env_vol >= op->DL)
						op->state = EG_SUS;
				}
			}
			break;
		case EG_SUS:	// sustain phase
			rate = ymf278b_slot_compute_rate(op, op->D2R);
			if (rate < 4)
				break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];

				if ((op->env_vol > dl_tab[6]) && op->PRVB)
					op->state = EG_REV;
				else
				{
					if (op->env_vol >= MAX_ATT_INDEX)
					{
						op->env_vol = MAX_ATT_INDEX;
						op->active = 0;
					}
				}
			}
			break;
		case EG_REL:	// release phase
			rate = ymf278b_slot_compute_rate(op, op->RR);
			if (rate < 4)
				break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];

				if ((op->env_vol > dl_tab[6]) && op->PRVB)
					op->state = EG_REV;
				else
				{
					if (op->env_vol >= MAX_ATT_INDEX)
					{
						op->env_vol = MAX_ATT_INDEX;
						op->active = 0;
					}
				}
			}
			break;
		case EG_REV:	// pseudo reverb
			// TODO improve env_vol update
			rate = ymf278b_slot_compute_rate(op, 5);
			//if (rate < 4)
			//	break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];

				if (op->env_vol >= MAX_ATT_INDEX)
				{
					op->env_vol = MAX_ATT_INDEX;
					op->active = 0;
				}
			}
			break;
		case EG_DMP:	// damping
			// TODO improve env_vol update, damp is just fastest decay now
			rate = 56;
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];

				if (op->env_vol >= MAX_ATT_INDEX)
				{
					op->env_vol = MAX_ATT_INDEX;
					op->active = 0;
				}
			}
			break;
		case EG_OFF:
			// nothing
			break;

		default:
			//UNREACHABLE;
			break;
		}
	}
}

INLINE INT16 ymf278b_getSample(YMF278BChip* chip, YMF278BSlot* op)
{
	// TODO How does this behave when R#2 bit 0 = 1?
	//      As-if read returns 0xff? (Like for CPU memory reads.) Or is
	//      sound generation blocked at some higher level?
	INT16 sample;
	UINT32 addr;
	UINT8* addrp;
	
	switch (op->bits)
	{
	case 0:
		// 8 bit
		sample = ymf278b_readMem(chip, op->startaddr + op->pos) << 8;
		break;
	case 1:
		// 12 bit
		addr = op->startaddr + ((op->pos / 2) * 3);
		addrp = ymf278b_getMemPtr(chip, addr);
		if (op->pos & 1)
			sample = (addrp[2] << 8) | ((addrp[1] << 4) & 0xF0);
		else
			sample = (addrp[0] << 8) | (addrp[1] & 0xF0);
		break;
	case 2:
		// 16 bit
		addr = op->startaddr + (op->pos * 2);
		addrp = ymf278b_getMemPtr(chip, addr);
		if (addrp == NULL)
			return 0;
		sample = (addrp[0] << 8) | addrp[1];
		break;
	default:
		// TODO unspecified
		sample = 0;
		break;
	}
	return sample;
}

static int ymf278b_anyActive(YMF278BChip* chip)
{
	int i;
	
	for (i = 0; i < 24; i ++)
	{
		if (chip->slots[i].active)
			return 1;
	}
	return 0;
}

static void ymf278b_pcm_update(void *info, UINT32 samples, DEV_SMPL** outputs)
{
	YMF278BChip* chip = (YMF278BChip *)info;
	int i;
	UINT32 j;
	INT32 vl;
	INT32 vr;
	
	memset(outputs[0], 0x00, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0x00, samples * sizeof(DEV_SMPL));
	
	if (! ymf278b_anyActive(chip))
	{
		// TODO update internal state, even if muted
		// TODO also mute individual channels
		return;
	}

	vl = mix_level[chip->pcm_l];
	vr = mix_level[chip->pcm_r];
	for (j = 0; j < samples; j ++)
	{
		for (i = 0; i < 24; i ++)
		{
			YMF278BSlot* sl;
			INT16 sample;
			int vol;
			int volLeft;
			int volRight;
			UINT32 step;
			
			sl = &chip->slots[i];
			if (! sl->active || sl->Muted)
			{
				//outputs[0][j] += 0;
				//outputs[1][j] += 0;
				continue;
			}

			sample = (sl->sample1 * (0x10000 - sl->stepptr) +
			          sl->sample2 * sl->stepptr) >> 16;
			vol = sl->TL + (sl->env_vol >> 2) + ymf278b_slot_compute_am(sl);
			if (vol < 0)
				vol = 0;	// clip negative values (can occour due to AM)

			volLeft  = vol + pan_left [sl->pan] + vl;
			volRight = vol + pan_right[sl->pan] + vr;
			// verified on HW: internal TL levels above 0x7F are cut to silence
			// TODO: test how envelope + pan + master volume affects this (only [sl->TL > 0x7F] was tested)
			volLeft  = (volLeft  < 0x80) ? chip->volume[volLeft ] : 0;
			volRight = (volRight < 0x80) ? chip->volume[volRight] : 0;

			outputs[0][j] += (sample * volLeft ) >> 17;
			outputs[1][j] += (sample * volRight) >> 17;

			step = (sl->lfo_active && sl->vib)
			     ? calcStep(sl->OCT, sl->FN, ymf278b_slot_compute_vib(sl))
			     : sl->step;
			sl->stepptr += step;

			while (sl->stepptr >= 0x10000)
			{
				sl->stepptr -= 0x10000;
				sl->sample1 = sl->sample2;
				
				sl->sample2 = ymf278b_getSample(chip, sl);
				if (sl->pos >= sl->endaddr)
					sl->pos = sl->pos - sl->endaddr + sl->loopaddr;
				else
					sl->pos ++;
			}
		}
		ymf278b_advance(chip);
	}
}

INLINE void ymf278b_keyOnHelper(YMF278BChip* chip, YMF278BSlot* slot)
{
	slot->active = 1;

	slot->state = EG_ATT;
	slot->stepptr = 0;
	slot->pos = 0;
	slot->sample1 = ymf278b_getSample(chip, slot);
	slot->pos = 1;
	slot->sample2 = ymf278b_getSample(chip, slot);
}

static void ymf278b_A_w(YMF278BChip *chip, UINT8 reg, UINT8 data)
{
	switch(reg)
	{
		case 0x02:
			//chip->timer_a_count = data;
			//ymf278b_timer_a_reset(chip);
			break;
		case 0x03:
			//chip->timer_b_count = data;
			//ymf278b_timer_b_reset(chip);
			break;
		case 0x04:
			/*if(data & 0x80)
				chip->current_irq = 0;
			else
			{
				UINT8 old_enable = chip->enable;
				chip->enable = data;
				chip->current_irq &= ~data;
				if((old_enable ^ data) & 1)
					ymf278b_timer_a_reset(chip);
				if((old_enable ^ data) & 2)
					ymf278b_timer_b_reset(chip);
			}
			ymf278b_irq_check(chip);*/
			break;
		default:
//			logerror("YMF278B:  Port A write %02x, %02x\n", reg, data);
			break;
	}
}

static void ymf278b_B_w(YMF278BChip *chip, UINT8 reg, UINT8 data)
{
	switch(reg)
	{
		case 0x05:	// OPL3/OPL4 Enable
			// Bit 1 enables OPL4 WaveTable Synth
			chip->exp = data;
			break;
		default:
			break;
	}
//#ifdef _DEBUG
//	logerror("YMF278B:  Port B write %02x, %02x\n", reg, data);
//#endif
}

static void ymf278b_C_w(YMF278BChip* chip, UINT8 reg, UINT8 data)
{
	// Handle slot registers specifically
	if (reg >= 0x08 && reg <= 0xF7)
	{
		int snum = (reg - 8) % 24;
		YMF278BSlot* slot = &chip->slots[snum];
		UINT8 wavetblhdr;
		UINT32 base;
		UINT8* buf;
		int i;
		
		switch((reg - 8) / 24)
		{
		case 0:
			slot->wave = (slot->wave & 0x100) | data;
			wavetblhdr = (chip->regs[2] >> 2) & 0x7;
			base = (slot->wave < 384 || ! wavetblhdr) ?
			       (slot->wave * 12) :
			       (wavetblhdr * 0x80000 + ((slot->wave - 384) * 12));
			// TODO What if R#2 bit 0 = 1?
			//      See also getSample()
			buf = ymf278b_getMemPtr(chip, base);
			if (buf == NULL)
				break;
			
			slot->bits = (buf[0] & 0xC0) >> 6;
			slot->startaddr = buf[2] | (buf[1] << 8) |
			                  ((buf[0] & 0x3F) << 16);
			slot->loopaddr = buf[4] + (buf[3] << 8);
			slot->endaddr  = ((buf[6] + (buf[5] << 8)) ^ 0xFFFF);
			for (i = 7; i < 12; ++i)
			{
				// Verified on real YMF278:
				// After tone loading, if you read these
				// registers, their value actually has changed.
				ymf278b_C_w(chip, 8 + snum + (i - 2) * 24, buf[i]);
			}
			
			if (chip->regs[reg + 0x60] & 0x080)
				ymf278b_keyOnHelper(chip, slot);
			break;
		case 1:
			slot->wave = (slot->wave & 0xFF) | ((data & 0x1) << 8);
			slot->FN = (slot->FN & 0x380) | (data >> 1);
			slot->step = calcStep(slot->OCT, slot->FN, 0);
			break;
		case 2:
			slot->FN = (slot->FN & 0x07F) | ((data & 0x07) << 7);
			slot->PRVB = ((data & 0x08) >> 3);
			slot->OCT =  ((data & 0xF0) >> 4);
			slot->step = calcStep(slot->OCT, slot->FN, 0);
			break;
		case 3:
			slot->TLdest = data >> 1;
			if (slot->TLdest == 0x7F)
				slot->TLdest = 0xFF;	// verified on HW via volume interpolation
			slot->LD = data & 0x1;

			if (slot->LD) {
				// directly change volume
				slot->TL = slot->TLdest;
			} else {
				// interpolate volume
			}
			break;
		case 4:
			if (data & 0x10)
			{
				// output to DO1 pin:
				// this pin is not used in moonsound
				// we emulate this by muting the sound
				slot->pan = 8; // both left/right -inf dB
			}
			else
				slot->pan = data & 0x0F;

			if (data & 0x020)
			{
				// LFO reset
				slot->lfo_active = 0;
				slot->lfo_cnt = 0;
				slot->lfo_max = lfo_period[slot->vib];
				slot->lfo_step = 0;
			}
			else
			{
				// LFO activate
				slot->lfo_active = 1;
			}

			switch (data >> 6)
			{
			case 0: // tone off, no damp
				if (slot->active && (slot->state != EG_REV))
					slot->state = EG_REL;
				break;
			case 2: // tone on, no damp
				// 'Life on Mars' bug fix:
				//    In case KEY=ON + DAMP (value 0xc0) and we reach
				//    'env_vol == MAX_ATT_INDEX' (-> slot->active = false)
				//    we didn't trigger keyOnHelper() because KEY didn't
				//    change OFF->ON. Fixed by also checking slot.state.
				// TODO real HW is probably simpler because EG_DMP is not
				// an actual state, nor is 'slot->active' stored.
				if (!slot->active || !(chip->regs[reg] & 0x080))
					ymf278b_keyOnHelper(chip, slot);
				break;
			case 1: // tone off, damp
			case 3: // tone on,  damp
				slot->state = EG_DMP;
				break;
			}
			break;
		case 5:
			slot->vib = data & 0x7;
			ymf278b_slot_set_lfo(slot, (data >> 3) & 0x7);
			break;
		case 6:
			slot->AR  = data >> 4;
			slot->D1R = data & 0xF;
			break;
		case 7:
			slot->DL  = dl_tab[data >> 4];
			slot->D2R = data & 0xF;
			break;
		case 8:
			slot->RC = data >> 4;
			slot->RR = data & 0xF;
			break;
		case 9:
			slot->AM = data & 0x7;
			break;
		}
	}
	else
	{
		// All non-slot registers
		switch (reg)
		{
		case 0x00: // TEST
		case 0x01:
			break;

		case 0x02:
			// wave-table-header / memory-type / memory-access-mode
			// Simply store in regs[2]
			break;

		case 0x03:
			// Verified on real YMF278:
			// * Don't update the 'memadr' variable on writes to
			//   reg 3 and 4. Only store the value in the 'regs'
			//   array for later use.
			// * The upper 2 bits are not used to address the
			//   external memories (so from a HW pov they don't
			//   matter). But if you read back this register, the
			//   upper 2 bits always read as '0' (even if you wrote
			//   '1'). So we mask the bits here already.
			data &= 0x3F;
			break;

		case 0x04:
			// See reg 3.
			break;

		case 0x05:
			// Verified on real YMF278: (see above)
			// Only writes to reg 5 change the (full) 'memadr'.
			chip->memadr = (chip->regs[3] << 16) | (chip->regs[4] << 8) | data;
			break;

		case 0x06:  // memory data
			if (chip->regs[2] & 1) {
				ymf278b_writeMem(chip, chip->memadr, data);
				++chip->memadr; // no need to mask (again) here
			} else {
				// Verified on real YMF278:
				//  - writes are ignored
				//  - memadr is NOT increased
			}
			break;

		case 0xF8:
			chip->fm_l = data & 0x7;
			chip->fm_r = (data >> 3) & 0x7;
			refresh_opl3_volume(chip);
			break;

		case 0xF9:
			chip->pcm_l = data & 0x7;
			chip->pcm_r = (data >> 3) & 0x7;
			break;
		}
	}

	chip->regs[reg] = data;
}

static UINT8 ymf278b_readReg(YMF278BChip* chip, UINT8 reg)
{
	// no need to call updateStream(time)
	UINT8 result;
	switch(reg)
	{
	case 2: // 3 upper bits are device ID
		result = (chip->regs[2] & 0x1F) | 0x20;
		break;

	case 6: // Memory Data Register
		if (chip->regs[2] & 1)
		{
			result = ymf278b_readMem(chip, chip->memadr);
			// Verified on real YMF278:
			// memadr is only increased when 'regs[2] & 1'
			++chip->memadr; // no need to mask (again) here
		}
		else
		{
			// Verified on real YMF278
			result = 0xff;
		}
		break;

	default:
		result = chip->regs[reg];
		break;
	}
	
	return result;
}

static UINT8 ymf278b_readStatus(YMF278BChip* chip)
{
	UINT8 result = 0;
	//if (time < busyTime)
	//	result |= 0x01;
	//if (time < loadTime)
	//	result |= 0x02;
	return result;
}

static UINT8 ymf278b_r(void *info, UINT8 offset)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	UINT8 ret = 0;

	switch (offset)
	{
		// status register
		case 0:
		{
			// bits 0 and 1 are only valid if NEW2 is set
			//UINT8 newbits = 0;
			//if (chip->exp & 2)
			//	newbits = (chip->status_ld << 1) | chip->status_busy;

			//ret = newbits | chip->current_irq | (chip->irq_line == ASSERT_LINE ? 0x80 : 0x00);
			ret = ymf278b_readStatus(chip);
			break;
		}

		// FM regs can be read too (on contrary to what the datasheet says)
		case 1:
		case 3:
			// but they're not implemented here yet
			// This may be incorrect, but it makes the mbwave moonsound detection in msx drivers pass.
			ret = chip->last_fm_data;
			break;

		// PCM regs
		case 5:
			// only accessible if NEW2 is set
			if (~chip->exp & 2)
				break;

			ret = ymf278b_readReg(chip, chip->port_C);
			break;

		default:
			logerror("YMF278B: unexpected read at offset %X from ymf278b\n", offset);
			break;
	}

	return ret;
}

static void ymf278b_w(void *info, UINT8 offset, UINT8 data)
{
	YMF278BChip *chip = (YMF278BChip *)info;

	switch (offset)
	{
		case 0:
		case 2:
			chip->port_AB = data;
			chip->lastport = (offset>>1) & 1;
			chip->fm.write(chip->fm.chip, offset, data);
			break;

		case 1:
		case 3:
			chip->last_fm_data = data;
			chip->fm.write(chip->fm.chip, offset, data);
			if (! chip->lastport)
				ymf278b_A_w(chip, chip->port_AB, data);
			else
				ymf278b_B_w(chip, chip->port_AB, data);
			break;

		case 4:
			chip->port_C = data;
			break;

		case 5:
			// PCM regs are only accessible if NEW2 is set
			if (~chip->exp & 2)
				break;
			
			ymf278b_C_w(chip, chip->port_C, data);
			break;

		default:
#ifdef _DEBUG
			logerror("YMF278B: unexpected write at offset %X to ymf278b = %02X\n", offset, data);
#endif
			break;
	}
}

// This routine translates an address from the (upper) MoonSound address space
// to an address inside the (linearized) SRAM address space.
//
// The following info is based on measurements on a real MoonSound (v2.0)
// PCB. This PCB can have several possible SRAM configurations:
//   128kB:
//    1 SRAM chip of 128kB, chip enable (/CE) of this SRAM chip is connected to
//    the 1Y0 output of a 74LS139 (2-to-4 decoder). The enable input of the
//    74LS139 is connected to YMF278 pin /MCS6 and the 74LS139 1B:1A inputs are
//    connected to YMF278 pins MA18:MA17. So the SRAM is selected when /MC6 is
//    active and MA18:MA17 == 0:0.
//   256kB:
//    2 SRAM chips of 128kB. First one connected as above. Second one has /CE
//    connected to 74LS139 pin 1Y1. So SRAM2 is selected when /MSC6 is active
//    and MA18:MA17 == 0:1.
//   512kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//   640kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 128kB, /CE connected to /MCS7.
//      (This means SRAM2 is potentially mirrored over a 512kB region)
//  1024kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 512kB, /CE connected to /MCS7
//  2048kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 512kB, /CE connected to /MCS7
//    1 SRAM chip of 512kB, /CE connected to /MCS8
//    1 SRAM chip of 512kB, /CE connected to /MCS9
//      This configuration is not so easy to create on the v2.0 PCB. So it's
//      very rare.
//
// So the /MCS6 and /MCS7 (and /MCS8 and /MCS9 in case of 2048kB) signals are
// used to select the different SRAM chips. The meaning of these signals
// depends on the 'memory access mode'. This mode can be changed at run-time
// via bit 1 in register 2. The following table indicates for which regions
// these signals are active (normally MoonSound should be used with mode=0):
//              mode=0              mode=1
//  /MCS6   0x200000-0x27FFFF   0x380000-0x39FFFF
//  /MCS7   0x280000-0x2FFFFF   0x3A0000-0x3BFFFF
//  /MCS8   0x300000-0x37FFFF   0x3C0000-0x3DFFFF
//  /MCS9   0x380000-0x3FFFFF   0x3E0000-0x3FFFFF
//
// (For completeness) MoonSound also has 2MB ROM (YRW801), /CE of this ROM is
// connected to YMF278 /MCS0. In both mode=0 and mode=1 this signal is active
// for the region 0x000000-0x1FFFFF. (But this routine does not handle ROM).
UINT32 ymf278b_getRamAddress(YMF278BChip* chip, UINT32 addr)
{
	if (chip->regs[2] & 2) {
		// Normally MoonSound is used in 'memory access mode = 0'. But
		// in the rare case that mode=1 we adjust the address.
		if ((addr & 0x180000) == 0x180000) {
			addr &= ~0x180000;
			switch (addr & 0x060000) {
			case 0x000000: // [0x380000-0x39FFFF]
				// 1st 128kB of SRAM1
				break;
			case 0x020000: // [0x3A0000-0x3BFFFF]
				if (chip->RAMSize == 256 * 1024) {
					// 2nd 128kB SRAM chip
				} else {
					// 2nd block of 128kB in SRAM2
					// In case of 512+128, we use mirroring
					addr |= 0x080000;
				}
				break;
			case 0x040000: // [0x3C0000-0x3DFFFF]
				// 3rd 128kB block in SRAM3
				addr |= 0x100000;
				break;
			case 0x060000: // [0x3EFFFF-0x3FFFFF]
				// 4th 128kB block in SRAM4
				addr |= 0x180000;
				break;
			}
		} else {
			return (UINT32)-1; // unmapped
		}
	}
	if (chip->RAMSize == 640 * 1024) {
		// Verified on real MoonSound cartridge (v2.0): In case of
		// 640kB (1x512kB + 1x128kB), the 128kB SRAM chip is 4 times
		// visible. None of the other SRAM configurations show similar
		// mirroring (because the others are powers of two).
		if (addr & 0x080000) {
			addr &= ~0x060000;
		}
	}
	return addr;
}

INLINE UINT8* ymf278b_getMemPtr(YMF278BChip* chip, UINT32 address)
{
	address &= 0x3FFFFF;
	if (address < chip->ROMSize)
		return &chip->rom[address];
	address = ymf278b_getRamAddress(chip, address - chip->ROMSize);
	if (address < chip->RAMSize)
		return &chip->ram[address];
	else
		return NULL;
}

INLINE UINT8 ymf278b_readMem(YMF278BChip* chip, UINT32 address)
{
	// Verified on real YMF278: address space wraps at 4MB.
	address &= 0x3FFFFF;
	if (address < chip->ROMSize)	// ROM connected to /MCS0
		return chip->rom[address];
	address = ymf278b_getRamAddress(chip, address - chip->ROMSize);
	if (address < chip->RAMSize)
		return chip->ram[address];
	else	// unmapped region
		return 0xFF; // TODO check
}

INLINE void ymf278b_writeMem(YMF278BChip* chip, UINT32 address, UINT8 value)
{
	address &= 0x3FFFFF;
	if (address < chip->ROMSize)
		return; // can't write to ROM
	address = ymf278b_getRamAddress(chip, address - chip->ROMSize);
	if (address < chip->RAMSize)
		chip->ram[address] = value;
	else
		return;	// can't write to unmapped memory
	return;
}

static void opl3dummy_write(void* param, UINT8 address, UINT8 data)
{
	return;
}

static void opl3dummy_reset(void* param)
{
	return;
}

#define OPL4FM_VOL_BALANCE	0x16A	// 0x100 = 100%, FM is 3 db louder than PCM
static void refresh_opl3_volume(YMF278BChip* chip)
{
	INT32 volL, volR;
	
	if (chip->fm.setVol == NULL)
		return;
	
	volL = mix_level[chip->fm_l];
	volR = mix_level[chip->fm_r];
	// chip->volume[] uses 0x8000 = 100%
	volL = (chip->volume[volL] * OPL4FM_VOL_BALANCE) >> 7;
	volR = (chip->volume[volR] * OPL4FM_VOL_BALANCE) >> 7;
	chip->fm.setVol(chip->fm.chip, volL, volR);
	
	return;
}

static void init_opl3_devinfo(DEV_INFO* devInf, const DEV_GEN_CFG* baseCfg)
{
	DEVLINK_INFO* devLink;
	
	devInf->linkDevCount = 1;
	devInf->linkDevs = (DEVLINK_INFO*)calloc(devInf->linkDevCount, sizeof(DEVLINK_INFO));
	
	devLink = &devInf->linkDevs[0];
	devLink->devID = DEVID_YMF262;
	devLink->linkID = LINKDEV_OPL3;
	
	devLink->cfg = (DEV_GEN_CFG*)calloc(1, sizeof(DEV_GEN_CFG));
	*devLink->cfg = *baseCfg;
	devLink->cfg->clock = baseCfg->clock * 8 / 19;	// * 288 / 684
	devLink->cfg->emuCore = 0;
	
	return;
}

static UINT8 device_start_ymf278b(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	YMF278BChip *chip;
	UINT32 rate;
	UINT32 i;

	chip = (YMF278BChip *)calloc(1, sizeof(YMF278BChip));
	if (chip == NULL)
		return 0xFF;
	
	chip->clock = cfg->clock;
	rate = chip->clock / 768;
	//SRATE_CUSTOM_HIGHEST(cfg->srMode, rate, cfg->smplRate);
	
	device_ymf278b_link_opl3(chip, LINKDEV_OPL3, NULL);
	
	//chip->timer_a = timer_alloc(device->machine, ymf278b_timer_a_tick, chip);
	//chip->timer_b = timer_alloc(device->machine, ymf278b_timer_b_tick, chip);

	chip->ROMSize = 0;
	chip->rom = NULL;
	chip->RAMSize = 0;
	chip->ram = NULL;

	chip->memadr = 0; // avoid UMR

	// Volume table, 1 = -0.375dB, 8 = -3dB, 256 = -96dB
	// Note: The base of 2^-0.5 is applied to keep the volume of the original implementation.
	for (i = 0x00; i < 0x80; i ++)
		chip->volume[i] = (INT32)(32768 * pow(2.0, -0.5 + (-0.375 / 6) * i));

	ymf278b_set_mute_mask(chip, 0x000000);

	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, rate, &devDef);
	init_opl3_devinfo(retDevInf, cfg);

	return 0x00;
}

static void device_stop_ymf278b(void *info)
{
	YMF278BChip* chip = (YMF278BChip *)info;
	
	free(chip->ram);
	free(chip->rom);
	free(chip);
	
	return;
}

static void device_reset_ymf278b(void *info)
{
	YMF278BChip* chip = (YMF278BChip *)info;
	int i;
	
	chip->fm.reset(chip->fm.chip);
	chip->exp = 0x00;
	
	chip->eg_cnt = 0;
	chip->tl_int_cnt = 0;
	chip->tl_int_step = 0;

	for (i = 0; i < 24; i ++)
		ymf278b_slot_reset(&chip->slots[i]);
	chip->regs[2] = 0; // avoid UMR
	for (i = 255; i >= 0; i --)	// reverse order to avoid UMR
		ymf278b_C_w(chip, i, 0);
	
	chip->memadr = 0;
	chip->fm_l = chip->fm_r = 3;
	chip->pcm_l = chip->pcm_r = 0;
	refresh_opl3_volume(chip);
}

static UINT8 get_opl3_funcs(const DEV_DEF* devDef, OPL3FM* retFuncs)
{
	UINT8 retVal;
	
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&retFuncs->write);
	if (retVal)
		return retVal;
	
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_VOLUME_LR | RWF_WRITE, DEVRW_VALUE, 0, (void**)&retFuncs->setVol);
	if (retVal)
	{
		retFuncs->setVol = NULL;
		logerror("YMF278B Warning: Unable to control OPL3 volume.\n");
	}
	
	if (devDef->Reset == NULL)
		return 0xFF;
	retFuncs->reset = devDef->Reset;
	return 0x00;
}

static UINT8 device_ymf278b_link_opl3(void* param, UINT8 devID, const DEV_INFO* defInfOPL3)
{
	YMF278BChip* chip = (YMF278BChip *)param;
	UINT8 retVal;
	
	if (devID != LINKDEV_OPL3)
		return EERR_UNK_DEVICE;
	
	if (defInfOPL3 == NULL)
	{
		chip->fm.chip = NULL;
		chip->fm.write = opl3dummy_write;
		chip->fm.reset = opl3dummy_reset;
		chip->fm.setVol = NULL;
		retVal = 0x00;
	}
	else
	{
		retVal = get_opl3_funcs(defInfOPL3->devDef, &chip->fm);
		if (! retVal)
			chip->fm.chip = defInfOPL3->dataPtr;
		refresh_opl3_volume(chip);
	}
	return retVal;
}

static void ymf278b_alloc_rom(void* info, UINT32 memsize)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	
	if (chip->ROMSize == memsize)
		return;
	
	chip->rom = (UINT8*)realloc(chip->rom, memsize);
	chip->ROMSize = memsize;
	memset(chip->rom, 0xFF, memsize);
	
	return;
}

static void ymf278b_alloc_ram(void* info, UINT32 memsize)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	
	if (chip->RAMSize == memsize)
		return;
	
	chip->ram = (UINT8*)realloc(chip->ram, memsize);
	chip->RAMSize = memsize;
	memset(chip->ram, 0, chip->RAMSize);
	
	return;
}

static void ymf278b_write_rom(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	
	if (offset > chip->ROMSize)
		return;
	if (offset + length > chip->ROMSize)
		length = chip->ROMSize - offset;
	
	memcpy(chip->rom + offset, data, length);
	
	return;
}

static void ymf278b_write_ram(void *info, UINT32 offset, UINT32 length, const UINT8* data)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	
	if (offset > chip->RAMSize)
		return;
	if (offset + length > chip->RAMSize)
		length = chip->RAMSize - offset;
	
	memcpy(chip->ram + offset, data, length);
	
	return;
}


static void ymf278b_set_mute_mask(void *info, UINT32 MuteMask)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 24; CurChn ++)
		chip->slots[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
