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

/*
	Improved by Valley Bell, 2018
	Thanks to niekniek and l_oliveira for providing recordings from OPL4 hardware.
	Thanks to superctr and wouterv for discussing changes.

	Improvements:
		- added TL interpolation, recordings show that internal TL levels are 0x00..0xFF
		- fixed ADSR speeds, attack rate 15 is now instant
		- correct clamping of intermediate Rate Correction values
		- emulation of "loop glitch" (going out-of-bounds by playing a sample faster than it the loop is long)
		- made calculation of sample position cleaner and closer to how the HW works
		- increased output resolution from TL (0.375 db) to envelope (0.09375 db)
		- fixed volume table - 6 db steps are done using bit shifts, steps in between are multiplicators
		- made octave -8 freeze the sample
		- verified that TL and envelope levels are applied separately, both go silent at -60 db
		- implemented pseudo-reverb and damping according to manual
		- made pseudo-reverb ignore Rate Correction (real hardware ignores it)
		- reimplemented LFO, speed exactly matches the formulas that were probably used when creating the manual
		- fixed LFO tremolo (amplitude modulation)
		- made LFO vibrato and tremolo accurate to hardware

	Known issues:
		- Octave -8 was only tested with fnum 0. Other fnum values might behave differently.

	A note about attack/decay/release times in the OPL4 manual:
		The general formula used to calculate the times is:
			samples = baseValue / pow(2, floor(rate / 4)) / ((4 + (rate % 4)) / 8)
			msec = samples / 44.1

		attack time (exponential db):
			- 0 ~ 100%, values 4-48
			  baseValue = 0x43000
			- 10 ~ 90%, values 4-48
			  baseValue = 0x28000
		decay/release time (linear db):
			- 0 ~ 100% equals 0 db .. -90 db, which is 15 "steps" of -6 db change
			  time for -6 db = (value of 0~100%) / 15
			  baseValue = 0x3C0000
			- 10 ~ 90% is the time for going from envelope level 0 to 205
			  time for -6 db = (value of 10~90%) / (205/64)
			  baseValue = 0x0CD000
		
		The formulas above seem pretty accurate for low rates and slightly off for high rates.
		I assume it's due to rounding.
		The value for "attack rate, time 10~90%, rate 58" is wrong and should be 0.31.
*/

// Based on ymf278b.c written by R. Belmont and O. Galibert

// This class doesn't model a full YMF278b chip. Instead it only models the
// wave part. The FM part in modeled in YMF262 (it's almost 100% compatible,
// the small differences are handled in YMF262). The status register and
// interaction with the FM registers (e.g. the NEW2 bit) is currently handled
// in the MSXMoonSound class.

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../SoundDevs.h"
#include "../SoundEmu.h"
#include "../EmuHelper.h"
#include "../logging.h"
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
static void ymf278b_set_log_cb(void *info, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ymf278b_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ymf278b_r},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x524F, ymf278b_write_rom},	// 0x524F = 'RO' for ROM
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x524F, ymf278b_alloc_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x5241, ymf278b_write_ram},	// 0x5241 = 'RA' for RAM
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x5241, ymf278b_alloc_ram},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, ymf278b_set_mute_mask},
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
	ymf278b_set_log_cb,	// SetLoggingCallback
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
	UINT16 endaddr;	// Note: stored in 2s complement (0x0000 = 0, 0x0001 = -65535, 0xFFFF = -1)
	UINT32 step;	// fixed-point frequency step
					// invariant: step == calcStep(OCT, FN)
	UINT32 stepptr;	// fixed-point pointer into the sample
	UINT16 pos;

	INT16 env_vol;

	UINT32 lfo_cnt;	// LFO state counter

	INT16 DL;
	UINT16 wave;	// wavetable number
	UINT16 FN;		// f-number         TODO store 'FN | 1024'?
	INT8 OCT;		// octave [-8..+7]
	UINT8 PRVB;		// pseudo-reverb
	UINT8 LD;		// level direct
	UINT8 TLdest;	// destination total level
	UINT8 TL;		// total level
	UINT8 pan;		// panpot
	UINT8 keyon;	// slot keyed on
	UINT8 DAMP;
	UINT8 lfo;		// LFO
	UINT8 vib;		// vibrato
	UINT8 AM;		// AM level
	UINT8 AR;
	UINT8 D1R;
	UINT8 D2R;
	UINT8 RC;		// rate correction
	UINT8 RR;

	UINT8 bits;		// width of the samples

	UINT8 state;	// envelope generator state
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
	DEV_LOGGER logger;

	YMF278BSlot slots[24];

	/** Global envelope generator counter. */
	UINT32 eg_cnt;
	UINT32 tl_int_cnt;
	UINT8 tl_int_step;	// modulo 3 counter

	UINT32 memadr;

	INT32 fm_l, fm_r;
	INT32 pcm_l, pcm_r;

	UINT32 ROMSize;
	UINT8 *rom;
	UINT32 RAMSize;
	UINT8 *ram;
	UINT32 clock;

	UINT8 regs[0x100];

	UINT8 exp;

	UINT8 port_AB, port_C, lastport;

	UINT8 last_fm_data;
	OPL3FM fm;
};

INLINE UINT8* ymf278b_getMemPtr(YMF278BChip* chip, UINT32 address);
INLINE UINT8 ymf278b_readMem(YMF278BChip* chip, UINT32 address);
INLINE void ymf278b_writeMem(YMF278BChip* chip, UINT32 address, UINT8 value);


// envelope output entries
// fixed to match recordings from actual OPL4 -Valley Bell
#define ENV_BITS			10
#define ENV_LEN				(1 << ENV_BITS)

#define MAX_ATT_INDEX		0x280	// makes attack phase right and also goes well with "envelope stops at -60 db"
#define MIN_ATT_INDEX		0

#define TL_SHIFT			2	// envelope values are 4x as fine as TL levels

#define LFO_SHIFT			18 // LFO period of up to 0x40000 samples
#define LFO_PERIOD			(1 << LFO_SHIFT)

// Envelope Generator phases
#define EG_ATT	4
#define EG_DEC	3
#define EG_SUS	2
#define EG_REL	1
#define EG_OFF	0

// Pan values, units are -3dB, i.e. 8.
static const INT32 pan_left[16]  = {
	0, 8, 16, 24, 32, 40, 48, 255, 255,   0,  0,  0,  0,  0,  0, 0
};
static const INT32 pan_right[16] = {
	0, 0,  0,  0,  0,  0,  0,   0, 255, 255, 48, 40, 32, 24, 16, 8
};

// Mixing levels, units are -3dB
static const INT32 mix_level[8] = {
	0, 8, 16, 24, 32, 40, 48, 255
};

// Precalculated attenuation values
static INT32 vol_tab[ENV_LEN];

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
#define SC(db) (INT16)(db / 3 * 0x20)
static const INT16 dl_tab[16] = {
 SC( 0), SC( 3), SC( 6), SC( 9), SC(12), SC(15), SC(18), SC(21),
 SC(24), SC(27), SC(30), SC(33), SC(36), SC(39), SC(42), SC(93)
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
	O(14),O(14),O(14),O(14),
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


// number of steps the LFO counter advances per sample
#define O(a) (UINT32)(LFO_PERIOD * a / 44100.0 + 0.5)	// LFO frequency (Hz) -> LFO counter steps per sample
static const UINT32 lfo_period[8] = {
	O(0.168),	// step:  1, period: 262144 samples
	O(2.019),	// step: 12, period:  21845 samples
	O(3.196),	// step: 19, period:  13797 samples
	O(4.206),	// step: 25, period:  10486 samples
	O(5.215),	// step: 31, period:   8456 samples
	O(5.888),	// step: 35, period:   7490 samples
	O(6.224),	// step: 37, period:   7085 samples
	O(7.066)	// step: 42, period:   6242 samples
};
#undef O


// formula used by Yamaha docs:
//	vib_depth_cents(x) = (log2(0x400 + x) - 10) * 1200
static const INT16 vib_depth[8] = {
	0,	//  0.000 cents
	2,	//  3.378 cents
	3,	//  5.065 cents
	4,	//  6.750 cents
	6,	// 10.114 cents
	12,	// 20.170 cents
	24,	// 40.106 cents
	48,	// 79.307 cents
};


// formula used by Yamaha docs:
//	am_depth_db(x) = (x-1) / 0x40 * 6.0
//	They use (x-1), because the depth is multiplied with the AM counter, which has a range of 0..0x7F.
//	Thus the maximum attenuation with x=0x80 is (0x7F * 0x80) >> 7 = 0x7F.
// reversed formula:
//	am_depth(db) = round(db / 6.0 * 0x40) + 1
static const UINT8 am_depth[8] = {
	0x00,	//  0.000 db
	0x14,	//  1.781 db
	0x20,	//  2.906 db
	0x28,	//  3.656 db
	0x30,	//  4.406 db
	0x40,	//  5.906 db
	0x50,	//  7.406 db
	0x80,	// 11.910 db
};


static UINT8 tablesInit = 0;

// Sign extend a 4-bit value to 8-bit int
// require: x in range [0..15]
INLINE INT8 sign_extend_4(UINT8 x)
{
	return ((INT8)x ^ 8) - 8;
}

// Params: oct in [-8 ..   +7]
//         fn  in [ 0 .. 1023]
// We want to interpret oct as a signed 4-bit number and calculate
//    ((fn | 1024) + vib) << (5 + oct)
// Though in this formula the shift can go over a negative distance (in that
// case we should shift in the other direction).
INLINE UINT32 calcStep(INT8 oct, UINT16 fn, INT16 vib)
{
	if (oct == -8)
	{
		return 0;
	}
	else
	{
		UINT32 t = (fn + 1024 + vib) << (8 + oct); // use '+' iso '|' (generates slightly better code)
		return t >> 3; // was shifted 3 positions too far
	}
}

static void ymf278b_slot_reset(YMF278BSlot* slot)
{
	slot->wave = slot->FN = slot->OCT = slot->PRVB = slot->LD = slot->TLdest = slot->TL = slot->pan =
		slot->keyon = slot->DAMP = slot->lfo = slot->vib = slot->AM = 0;
	slot->DL = slot->AR = slot->D1R = slot->D2R = slot->RC = slot->RR = 0;
	slot->stepptr = 0;
	slot->step = calcStep(slot->OCT, slot->FN, 0);
	slot->bits = slot->startaddr = slot->loopaddr = slot->endaddr = 0;
	slot->env_vol = MAX_ATT_INDEX;

	slot->lfo_active = 0;
	slot->lfo_cnt = 0;

	slot->state = EG_OFF;

	// not strictly needed, but avoid UMR on savestate
	slot->pos = 0;
}

INLINE UINT8 ymf278b_slot_compute_rate(YMF278BSlot* slot, int val)
{
	int res;
	
	if (val == 0)
		return 0;
	else if (val == 15)
		return 63;
	
	res = val * 4;
	if (slot->RC != 15)
	{
		int oct_rc = slot->OCT + slot->RC;
		// clamping verified with HW tests -Valley Bell
		if (oct_rc < 0x0)
			oct_rc = 0x0;
		else if (oct_rc > 0xF)
			oct_rc = 0xF;
		res += oct_rc * 2 + ((slot->FN & 0x200) >> 9);
	}
	
	if (res < 0)
		res = 0;
	else if (res > 63)
		res = 63;
	
	return (UINT8)res;
}

INLINE UINT8 ymf278b_slot_compute_decay_rate(YMF278BSlot* slot, int val)
{
	if (slot->DAMP)
	{
		// damping
		// The manual lists these values for time and attenuation: (44100 samples/second)
		// -12 db at  5.8 ms, sample 256
		// -48 db at  8.0 ms, sample 352
		// -72 db at  9.4 ms, sample 416
		// -96 db at 10.9 ms, sample 480
		// This results in these durations and rate values for the respecitve phases:
		//   0 db .. -12 db: 256 samples (5.80 ms) -> 128 samples per -6 db = rate 48
		// -12 db .. -48 db:  96 samples (2.18 ms) ->  16 samples per -6 db = rate 63
		// -48 db .. -72 db:  64 samples (1.45 ms) ->  16 samples per -6 db = rate 63
		// -72 db .. -96 db:  64 samples (1.45 ms) ->  16 samples per -6 db = rate 63
		// Damping was verified to ignore rate correction.
		if (slot->env_vol < dl_tab[4])
			return 48;	// 0 db .. -12 db
		else
			return 63;	// -12 db .. -96 db
	}
	if (slot->PRVB)
	{
		// pseudo reverb
		// activated when reaching -18 db, overrides D1R/D2R/RR with reverb rate 5
		
		// The manual is actually a bit unclear and just says "RATE=5", referring to the D1R/D2R/RR register value.
		// However, later pages use "RATE" to refer to the "internal" rate, which is (register * 4) + rate correction.
		// HW recordings prove that Rate Correction is ignored, so pseudo reverb just sets the
		// "internal" rate to a value of 4*5 = 20.
		if (slot->env_vol >= dl_tab[6])
			return 20;
	}
	return ymf278b_slot_compute_rate(slot, val);
}

INLINE INT16 ymf278b_slot_compute_vib(YMF278BSlot* slot)
{
	// verified via hardware recording:
	//  With LFO speed 0 (period 262144 samples), each vibrato step takes 4096 samples.
	//  -> 64 steps total
	//  Also, with vibrato depth 7 (80 cents) and an F-Num of 0x400, the final F-Nums are:
	//  0x400 .. 0x43C, 0x43C .. 0x400, 0x400 .. 0x3C4, 0x3C4 .. 0x400
	INT16 lfo_fm = (INT16)(slot->lfo_cnt / (LFO_PERIOD / 0x40));
	// results in +0x00..+0x0F, +0x0F..+0x00, -0x00..-0x0F, -0x0F..-0x00
	if (lfo_fm & 0x10)
		lfo_fm ^= 0x1F;
	if (lfo_fm & 0x20)
		lfo_fm = -(lfo_fm & 0x0F);
	
	return (lfo_fm * vib_depth[slot->vib]) / 12;
}

INLINE UINT16 ymf278b_slot_compute_am(YMF278BSlot* slot)
{
	// verified via hardware recording:
	//  With LFO speed 0 (period 262144 samples), each tremolo step takes 1024 samples.
	//  -> 256 steps total
	UINT16 lfo_am = (UINT16)(slot->lfo_cnt / (LFO_PERIOD / 0x100));
	// results in 0x00..0x7F, 0x7F..0x00
	if (lfo_am & 0x80)
		lfo_am ^= 0xFF;
	
	return (lfo_am * am_depth[slot->AM]) >> 7;
}


INLINE void ymf278b_eg_phase_switch(YMF278BSlot* op)
{
	// Note: The real hardware probably does only 1 phase switch at once (see Nuked OPL3/OPN2),
	// but it does the check every time the envelope generator runs, even if env_vol is not modified.
	// In order to improve the performance a bit, we call this function only when env_vol
	// is modified and instead can do multiple phase switches at once.
	switch(op->state)
	{
	case EG_ATT:	// attack phase
		if (op->env_vol > MIN_ATT_INDEX)
			break;
		// op->env_vol <= MIN_ATT_INDEX
		op->env_vol = MIN_ATT_INDEX;
		op->state = EG_DEC;
		// fall through
	case EG_DEC:	// decay phase
		if (op->env_vol < op->DL)
			break;
		// op->env_vol >= op->DL
		op->state = EG_SUS;
		// fall through
	case EG_SUS:	// sustain phase
	case EG_REL:	// release phase
		if (op->env_vol < MAX_ATT_INDEX)
			break;
		// op->env_vol >= MAX_ATT_INDEX
		op->env_vol = MAX_ATT_INDEX;
		op->state = EG_OFF;
		break;
	case EG_OFF:
	default:
		break;
	}
	
	return;
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
			if (chip->tl_int_step == 0)
			{
				// decrease volume by one step every 27 samples
				if (op->TL < op->TLdest)
					op->TL ++;
			}
			else //if (chip->tl_int_step > 0)
			{
				// increase volume by one step every 13.5 samples
				if (op->TL > op->TLdest)
					op->TL --;
			}
		}

		if (op->lfo_active)
		{
			op->lfo_cnt += lfo_period[op->lfo];
			op->lfo_cnt &= (LFO_PERIOD - 1);
		}

		// Envelope Generator
		switch(op->state)
		{
		case EG_ATT:	// attack phase
			rate = ymf278b_slot_compute_rate(op, op->AR);
			// Verified by HW recording (and matches Nemesis' tests of the YM2612):
			// AR = 0xF during KeyOn results in instant switch to EG_DEC. (see keyOnHelper)
			// Setting AR = 0xF while the attack phase is in progress freezes the envelope.
			if (rate >= 63)
				break;
			
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				// >>4 makes the attack phase's shape match the actual chip -Valley Bell
				op->env_vol += (~op->env_vol * eg_inc[select + ((chip->eg_cnt >> shift) & 7)]) >> 4;
				ymf278b_eg_phase_switch(op);
			}
			break;
		case EG_DEC:	// decay phase
			rate = ymf278b_slot_compute_decay_rate(op, op->D1R);
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];
				ymf278b_eg_phase_switch(op);
			}
			break;
		case EG_SUS:	// sustain phase
			rate = ymf278b_slot_compute_decay_rate(op, op->D2R);
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];
				ymf278b_eg_phase_switch(op);
			}
			break;
		case EG_REL:	// release phase
			rate = ymf278b_slot_compute_decay_rate(op, op->RR);
			shift = eg_rate_shift[rate];
			if (! (chip->eg_cnt & ((1 << shift) - 1)))
			{
				select = eg_rate_select[rate];
				op->env_vol += eg_inc[select + ((chip->eg_cnt >> shift) & 7)];
				ymf278b_eg_phase_switch(op);
			}
			break;
		case EG_OFF:
			// nothing
			break;
		}
	}
}

INLINE INT16 ymf278b_getSample(YMF278BChip* chip, YMF278BSlot* slot, UINT16 pos)
{
	// TODO How does this behave when R#2 bit 0 = 1?
	//      As-if read returns 0xff? (Like for CPU memory reads.) Or is
	//      sound generation blocked at some higher level?
	INT16 sample;
	UINT32 addr;
	
	switch (slot->bits)
	{
	case 0:
		// 8 bit
		sample = ymf278b_readMem(chip, slot->startaddr + pos) << 8;
		break;
	case 1:
		// 12 bit
		addr = slot->startaddr + ((pos / 2) * 3);
		if (pos & 1)
			sample = (ymf278b_readMem(chip, addr + 2) << 8) | ((ymf278b_readMem(chip, addr + 1) & 0xF0) << 0);
		else
			sample = (ymf278b_readMem(chip, addr + 0) << 8) | ((ymf278b_readMem(chip, addr + 1) & 0x0F) << 4);
		break;
	case 2:
		// 16 bit
		addr = slot->startaddr + (pos * 2);
		sample = (ymf278b_readMem(chip, addr + 0) << 8) | ymf278b_readMem(chip, addr + 1);
		break;
	default:
		// TODO unspecified
		sample = 0;
		break;
	}
	return sample;
}

INLINE INT16 ymf278b_nextPos(YMF278BSlot* slot, UINT16 pos, UINT16 step)
{
	// If there is a 4-sample loop and you advance 12 samples per step,
	// it may exceed the end offset.
	// This is abused by the "Lizard Star" song to generate noise at 0:52. -Valley Bell
	pos += step;
	if ((UINT32)pos + slot->endaddr >= 0x10000)	// check position >= (negated) end address
		pos = pos + slot->endaddr + slot->loopaddr;	// This is how the actual chip does it.
	return pos;
}

static int ymf278b_anyActive(YMF278BChip* chip)
{
	int i;
	
	for (i = 0; i < 24; i ++)
	{
		if (chip->slots[i].state != EG_OFF)
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
	
	memset(outputs[0], 0, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0, samples * sizeof(DEV_SMPL));
	
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
			INT32 smplOut;
			UINT16 envVol;
			INT32 volLeft;
			INT32 volRight;
			UINT32 step;
			
			sl = &chip->slots[i];
			if (sl->state == EG_OFF || sl->Muted)
			{
				//outputs[0][j] += 0;
				//outputs[1][j] += 0;
				continue;
			}

			sample = (ymf278b_getSample(chip, sl, sl->pos) * (0x10000 - sl->stepptr) +
			          ymf278b_getSample(chip, sl, ymf278b_nextPos(sl, sl->pos, 1)) * sl->stepptr) >> 16;
			
			// TL levels are 00..FF internally (TL register value 7F is mapped to TL level FF)
			// Envelope levels have 4x the resolution (000..3FF)
			// Volume levels are approximate logarithmic: -6 db result in half volume. Steps in between use linear interpolation.
			// A volume of -60 db or lower results in silence. (value 0x280..0x3FF).
			// Recordings from actual hardware indicate, that TL level and envelope level are applied separately.
			// Each of them is clipped to silence below -60 db, but TL+envelope might result in a lower volume. -Valley Bell
			envVol = (UINT16)sl->env_vol;
			if (sl->lfo_active && sl->AM)
				envVol += ymf278b_slot_compute_am(sl);
			if (envVol >= MAX_ATT_INDEX)
				envVol = MAX_ATT_INDEX;
			smplOut = (sample * vol_tab[envVol]) >> 15;
			smplOut = (smplOut * vol_tab[sl->TL << TL_SHIFT]) >> 15;

			// Panning is also done separately. (low-volume TL + low-volume panning goes below -60 db)
			// I'll be taking wild guess and assume that -3 db is approximated with 75%. (same as with TL and envelope levels)
			// The same applies to the PCM mix level.
			volLeft  = pan_left [sl->pan] + vl;
			volRight = pan_right[sl->pan] + vr;
			// 0 -> 0x20, 8 -> 0x18, 16 -> 0x10, 24 -> 0x0C, etc. (not using vol_tab here saves array boundary checks)
			volLeft  = (0x20 - (volLeft  & 0x0F)) >> (volLeft  >> 4);
			volRight = (0x20 - (volRight & 0x0F)) >> (volRight >> 4);
			
			smplOut = (smplOut * 0x5A82) >> 17;	// reduce volume by -15 db, should bring it into balance with FM
			outputs[0][j] += (smplOut * volLeft ) >> 5;
			outputs[1][j] += (smplOut * volRight) >> 5;

			step = (sl->lfo_active && sl->vib)
			     ? calcStep(sl->OCT, sl->FN, ymf278b_slot_compute_vib(sl))
			     : sl->step;
			sl->stepptr += step;

			if (sl->stepptr >= 0x10000)
			{
				sl->pos = ymf278b_nextPos(sl, sl->pos, sl->stepptr >> 16);
				sl->stepptr &= 0xFFFF;
			}
		}
		ymf278b_advance(chip);
	}
}

INLINE void ymf278b_keyOnHelper(YMF278BChip* chip, YMF278BSlot* slot)
{
	UINT8 rate;

	// Unlike FM, the envelope level is reset. (And it makes sense, because you restart the sample.)
	slot->env_vol = MAX_ATT_INDEX;
	slot->state = EG_ATT;
	rate = ymf278b_slot_compute_rate(slot, slot->AR);
	if (rate >= 63)
	{
		// Nuke.YKT verified that the FM part does it exactly this way,
		// and the OPL4 manual says it's instant as well.
		slot->env_vol = MIN_ATT_INDEX;
		ymf278b_eg_phase_switch(slot);
	}
	slot->stepptr = 0;
	slot->pos = 0;
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
//			emu_logf(&chip->logger, DEVLOG_DEBUG, "Port A write %02x, %02x\n", reg, data);
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
			{
				slot->bits = ~0;	// set invalid value to mute the sample
				break;
			}
			
			slot->bits = (buf[0] & 0xC0) >> 6;
			slot->startaddr = buf[2] | (buf[1] << 8) | ((buf[0] & 0x3F) << 16);
			slot->loopaddr = buf[4] | (buf[3] << 8);
			slot->endaddr  = buf[6] | (buf[5] << 8);
			for (i = 7; i < 12; ++i)
			{
				// Verified on real YMF278:
				// After tone loading, if you read these
				// registers, their value actually has changed.
				ymf278b_C_w(chip, 8 + snum + (i - 2) * 24, buf[i]);
			}
			
			if (slot->keyon) {
				ymf278b_keyOnHelper(chip, slot);
			} else {
				slot->stepptr = 0;
				slot->pos = 0;
			}
			break;
		case 1:
			slot->wave = (slot->wave & 0xFF) | ((data & 0x1) << 8);
			slot->FN = (slot->FN & 0x380) | (data >> 1);
			slot->step = calcStep(slot->OCT, slot->FN, 0);
			break;
		case 2:
			slot->FN = (slot->FN & 0x07F) | ((data & 0x07) << 7);
			slot->PRVB = ((data & 0x08) >> 3);
			slot->OCT =  sign_extend_4((data & 0xF0) >> 4);
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

			if (data & 0x20)
			{
				// LFO reset
				slot->lfo_active = 0;
				slot->lfo_cnt = 0;
			}
			else
			{
				// LFO activate
				slot->lfo_active = 1;
			}

			slot->DAMP = (data & 0x40) >> 6;
			if (data & 0x80)
			{
				if (! slot->keyon)
				{
					slot->keyon = 1;
					ymf278b_keyOnHelper(chip, slot);
				}
			}
			else
			{
				if (slot->keyon)
				{
					slot->keyon = 0;
					slot->state = EG_REL;
				}
			}
			break;
		case 5:
			slot->lfo = (data >> 3) & 0x7;
			slot->vib = data & 0x7;
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
			emu_logf(&chip->logger, DEVLOG_DEBUG, "unexpected read at offset %X from ymf278b\n", offset);
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
			emu_logf(&chip->logger, DEVLOG_DEBUG, "unexpected write at offset %X to ymf278b = %02X\n", offset, data);
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
static UINT32 ymf278b_getRamAddress(YMF278BChip* chip, UINT32 addr)
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

#define OPL4FM_VOL_BALANCE	0x100	// 0x100 = 100%
static void refresh_opl3_volume(YMF278BChip* chip)
{
	INT32 volL, volR;
	
	if (chip->fm.setVol == NULL)
		return;
	
	volL = mix_level[chip->fm_l];
	volR = mix_level[chip->fm_r];
	// vol_tab[] uses 0x8000 = 100%
	volL = (vol_tab[volL << TL_SHIFT] * OPL4FM_VOL_BALANCE) >> 7;
	volR = (vol_tab[volR << TL_SHIFT] * OPL4FM_VOL_BALANCE) >> 7;
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

	if (! tablesInit)
	{
		tablesInit = 1;
		
		// Volume table (envelope levels)
		for (i = 0x00; i < ENV_LEN; i ++)
		{
			if (i < MAX_ATT_INDEX)
			{
				int vol_mul = 0x80 - (i & 0x3F);	// 0x40 values per 6 db
				int vol_shift = 7 + (i >> 6);		// approximation: -6 dB == divide by two (shift right)
				vol_tab[i] = (0x8000 * vol_mul) >> vol_shift;
			}
			else
			{
				// OPL4 hardware seems to clip to silence here below -60 db.
				vol_tab[i] = 0;
			}
		}
	}

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

static UINT8 get_opl3_funcs(const DEV_DEF* devDef, OPL3FM* retFuncs, DEV_LOGGER* logger)
{
	UINT8 retVal;
	
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&retFuncs->write);
	if (retVal)
		return retVal;
	
	retVal = SndEmu_GetDeviceFunc(devDef, RWF_VOLUME_LR | RWF_WRITE, DEVRW_VALUE, 0, (void**)&retFuncs->setVol);
	if (retVal)
	{
		retFuncs->setVol = NULL;
		emu_logf(logger, DEVLOG_WARN, "Unable to control OPL3 volume.\n");
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
		retVal = get_opl3_funcs(defInfOPL3->devDef, &chip->fm, &chip->logger);
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

static void ymf278b_set_log_cb(void *info, DEVCB_LOG func, void* param)
{
	YMF278BChip *chip = (YMF278BChip *)info;
	dev_logger_set(&chip->logger, chip, func, param);
	return;
}
