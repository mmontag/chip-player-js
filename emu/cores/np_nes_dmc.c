// Ported from NSFPlay to VGMPlay (including C++ -> C conversion)
// by Valley Bell on 25 September 2013
// Updated to NSFPlay 2.3 on 26 September 2013
// (Note: Encoding is UTF-8)

#include <stdlib.h>	// for rand
#include <stdlib.h>
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../../stdbool.h"
#include "../snddef.h"
#include "../RatioCntr.h"
#include "np_nes_apu.h"	// for NES_APU_np_FrameSequence
#include "np_nes_dmc.h"


// Master Clock: 21477272 (NTSC)
// APU Clock = Master Clock / 12
#define DEFAULT_CLOCK	1789772.0
#define DEFAULT_CLK_PAL	1662607
#define DEFAULT_RATE	44100


/** Bottom Half of APU **/
enum
{
	OPT_UNMUTE_ON_RESET=0,
	OPT_NONLINEAR_MIXER,
	OPT_ENABLE_4011,
	OPT_ENABLE_PNOISE,
	OPT_DPCM_ANTI_CLICK,
	OPT_RANDOMIZE_NOISE,
	OPT_TRI_MUTE,
	OPT_TRI_NULL,
	OPT_RANDOMIZE_TRI,
	OPT_DPCM_REVERSE,
	OPT_END
};


typedef struct _NES_DMC NES_DMC;
struct _NES_DMC
{
	DEV_DATA _devData;

	//const int GETA_BITS;
	//static const UINT32 freq_table[2][16];
	//static const UINT32 wavlen_table[2][16];
	UINT32 tnd_table[2][16][16][128];

	int option[OPT_END];
	int mask;
	INT32 sm[2][3];
	UINT8 reg[0x10];
	UINT32 len_reg;
	UINT32 adr_reg;
	//IDevice *memory;
	const UINT8* memory;
	UINT32 out[3];
	UINT32 daddress;
	UINT32 dlength;
	UINT32 data;
	bool empty;
	INT16 damp;
	int dac_lsb;
	bool dmc_pop;
	INT32 dmc_pop_offset;
	INT32 dmc_pop_follow;
	UINT32 clock;
	UINT32 rate;
	int pal;
	int mode;
	bool irq;

	INT32 counter[3];	// frequency dividers
	int tphase;			// triangle phase
	UINT32 nfreq;		// noise frequency
	UINT32 dfreq;		// DPCM frequency

	UINT32 tri_freq;
	int linear_counter;
	int linear_counter_reload;
	bool linear_counter_halt;
	bool linear_counter_control;

	int noise_volume;
	UINT32 noise, noise_tap;

	// noise envelope
	bool envelope_loop;
	bool envelope_disable;
	bool envelope_write;
	int envelope_div_period;
	int envelope_div;
	int envelope_counter;

	bool enable[2];	// tri/noise enable
	int length_counter[2];	// 0=tri, 1=noise

	// frame sequencer
	void* apu;	// apu is clocked by DMC's frame sequencer
	int frame_sequence_count;	// current cycle count
	int frame_sequence_length;	// CPU cycles per FrameSequence
	int frame_sequence_step;	// current step of frame sequence
	int frame_sequence_steps;	// 4/5 steps per frame
	bool frame_irq;
	bool frame_irq_enable;

	RATIO_CNTR tick_count;
};

INLINE UINT32 calc_tri(NES_DMC* dmc, UINT32 clocks);
INLINE UINT32 calc_dmc(NES_DMC* dmc, UINT32 clocks);
INLINE UINT32 calc_noise(NES_DMC* dmc, UINT32 clocks);
static void FrameSequence(NES_DMC* dmc, int s);
static void TickFrameSequence(NES_DMC* dmc, UINT32 clocks);
static void Tick(NES_DMC* dmc, UINT32 clocks);

#define GETA_BITS	20
static const UINT32 wavlen_table[2][16] = {
{	// NTSC
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
},
{	// PAL
	4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708,  944, 1890, 3778
}};

static const UINT32 freq_table[2][16] = {
{	// NTSC
	428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106,  84,  72,  54
},
{	// PAL
	398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118,  98,  78,  66,  50
}};

static const UINT32 BITREVERSE[256] = {
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
	0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
	0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
	0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
	0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
	0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
	0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
	0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
	0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF,
};

void* NES_DMC_np_Create(UINT32 clock, UINT32 rate)
{
	NES_DMC* dmc;
	int c, t;

	dmc = (NES_DMC*)calloc(1, sizeof(NES_DMC));
	if (dmc == NULL)
		return NULL;

	NES_DMC_np_SetClock(dmc, clock);
	NES_DMC_np_SetRate(dmc, rate);
	dmc->option[OPT_ENABLE_4011] = 1;
	dmc->option[OPT_ENABLE_PNOISE] = 1;
	dmc->option[OPT_UNMUTE_ON_RESET] = 1;
	dmc->option[OPT_DPCM_ANTI_CLICK] = 0;
	dmc->option[OPT_NONLINEAR_MIXER] = 1;
	dmc->option[OPT_RANDOMIZE_NOISE] = 1;
	dmc->option[OPT_RANDOMIZE_TRI] = 1;
	dmc->option[OPT_TRI_MUTE] = 1;
	dmc->option[OPT_TRI_NULL] = 0;
	dmc->option[OPT_DPCM_REVERSE] = 0;
	dmc->tnd_table[0][0][0][0] = 0;
	dmc->tnd_table[1][0][0][0] = 0;

	dmc->apu = NULL;
	dmc->frame_sequence_count = 0;
	dmc->frame_sequence_length = 7458;
	dmc->frame_sequence_steps = 4;

	for(c=0;c<2;++c)
		for(t=0;t<3;++t)
			dmc->sm[c][t] = 128;

	dmc->mask = 0;

	return dmc;
}


void NES_DMC_np_Destroy(void* chip)
{
	free(chip);
}

int NES_DMC_np_GetDamp(void* chip)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	return (dmc->damp<<1)|dmc->dac_lsb;
}

void NES_DMC_np_SetMask(void* chip, int m)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->mask = m;
}

void NES_DMC_np_SetStereoMix(void* chip, int trk, INT16 mixl, INT16 mixr)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	if (trk < 0) return;
	if (trk > 2) return;
	dmc->sm[0][trk] = mixl;
	dmc->sm[1][trk] = mixr;
}

static void FrameSequence(NES_DMC* dmc, int s)
{
	//DEBUG_OUT("FrameSequence: %d\n",s);

	if (s > 3) return;	// no operation in step 4

	if (dmc->apu != NULL)
	{
		NES_APU_np_FrameSequence(dmc->apu, s);
	}

	if (s == 0 && (dmc->frame_sequence_steps == 4))
	{
		if (dmc->frame_irq_enable) dmc->frame_irq = true;
		//cpu->UpdateIRQ(NES_CPU::IRQD_FRAME, dmc->frame_irq & dmc->frame_irq_enable);
	}

	// 240hz clock
	{
		bool divider;
		
		// triangle linear counter
		if (dmc->linear_counter_halt)
		{
			dmc->linear_counter = dmc->linear_counter_reload;
		}
		else
		{
			if (dmc->linear_counter > 0) --dmc->linear_counter;
		}
		if (!dmc->linear_counter_control)
		{
			dmc->linear_counter_halt = false;
		}

		// noise envelope
		divider = false;
		if (dmc->envelope_write)
		{
			dmc->envelope_write = false;
			dmc->envelope_counter = 15;
			dmc->envelope_div = 0;
		}
		else
		{
			++dmc->envelope_div;
			if (dmc->envelope_div > dmc->envelope_div_period)
			{
				divider = true;
				dmc->envelope_div = 0;
			}
		}
		if (divider)
		{
			if (dmc->envelope_loop && dmc->envelope_counter == 0)
				dmc->envelope_counter = 15;
			else if (dmc->envelope_counter > 0)
				--dmc->envelope_counter;
		}
	}

	// 120hz clock
	if ((s&1) == 0)
	{
		// triangle length counter
		if (!dmc->linear_counter_control && (dmc->length_counter[0] > 0))
			--dmc->length_counter[0];

		// noise length counter
		if (!dmc->envelope_loop && (dmc->length_counter[1] > 0))
			--dmc->length_counter[1];
	}

}

// 三角波チャンネルの計算 戻り値は0-15
UINT32 calc_tri(NES_DMC* dmc, UINT32 clocks)
{
	static UINT32 tritbl[32] =
	{
	  0, 1, 2, 3, 4, 5, 6, 7,
	  8, 9,10,11,12,13,14,15,
	 15,14,13,12,11,10, 9, 8,
	  7, 6, 5, 4, 3, 2, 1, 0
	};

	if (dmc->linear_counter > 0 && dmc->length_counter[0] > 0
		&& (!dmc->option[OPT_TRI_MUTE] || dmc->tri_freq > 0))
	{
		dmc->counter[0] -= clocks;
		while (dmc->counter[0] < 0)
		{
			dmc->tphase = (dmc->tphase + 1) & 31;
			dmc->counter[0] += (dmc->tri_freq + 1);
		}
	}
	// additional option to prevent even more clicks -Valley Bell
	else if (dmc->option[OPT_TRI_NULL])
	{
		if (dmc->tphase > 0 && dmc->tphase < 31)
		{
			// Finish the Triangle wave to prevent clicks.
			dmc->counter[0] -= clocks;
			while(dmc->counter[0] < 0 && dmc->tphase < 31)
			{
				dmc->tphase = (dmc->tphase + 1) & 31;
				dmc->counter[0] += (dmc->tri_freq + 1);
			}
		}
	}

	//UINT32 ret = tritbl[dmc->tphase];
	//return ret;
	return tritbl[dmc->tphase];
}

// ノイズチャンネルの計算 戻り値は0-127
// 低サンプリングレートで合成するとエイリアスノイズが激しいので
// ノイズだけはこの関数内で高クロック合成し、簡易なサンプリングレート
// 変換を行っている。
UINT32 calc_noise(NES_DMC* dmc, UINT32 clocks)
{
	UINT32 last, count, accum, accum_clocks, average;
	UINT32 env = dmc->envelope_disable ? dmc->noise_volume : dmc->envelope_counter;
	if (dmc->length_counter[1] < 1) env = 0;

	last = (dmc->noise & 0x4000) ? 0 : env;
	if (clocks < 1) return last;

	// simple anti-aliasing (noise requires it, even when oversampling is off)
	count = 0;
	accum = dmc->counter[1] * last; // samples pending from previous calc
	accum_clocks = dmc->counter[1];
	#ifdef _DEBUG_NSFP
		INT32 start_clocks = dmc->counter[1];
	#endif
	if (dmc->counter[1] < 0) // only happens on startup when using the randomize noise option
	{
		accum = 0;
		accum_clocks = 0;
	}

	dmc->counter[1] -= clocks;
//	assert(dmc->nfreq > 0);	// prevent infinite loop
	if (dmc->nfreq <= 0)	// prevent infinite loop -VB
		return last;
	while (dmc->counter[1] < 0)
	{
		// tick the noise generator
		UINT32 feedback = (dmc->noise&1) ^ ((dmc->noise&dmc->noise_tap)?1:0);
		dmc->noise = (dmc->noise>>1) | (feedback<<14);

		last = (dmc->noise & 0x4000) ? 0 : env;
		accum += (last * dmc->nfreq);
		dmc->counter[1] += dmc->nfreq;
		++count;
		accum_clocks += dmc->nfreq;
	}

	if (count < 1) // no change over interval, don't anti-alias
	{
		return last;
	}

	accum -= (last * dmc->counter[1]); // remove these samples which belong in the next calc
	accum_clocks -= dmc->counter[1];
	#ifdef _DEBUG_NSFP
		if (start_clocks >= 0) assert(accum_clocks == clocks); // these should be equal
	#endif

	average = accum / accum_clocks;
//	assert(average <= 15); // above this would indicate overflow
	return average;
}

// DMCチャンネルの計算 戻り値は0-127
UINT32 calc_dmc(NES_DMC* dmc, UINT32 clocks)
{
	dmc->counter[2] -= clocks;
//	assert(dmc->dfreq > 0);	// prevent infinite loop
	if (dmc->dfreq <= 0)	// prevent infinite loop -VB
		return (dmc->damp<<1) + dmc->dac_lsb;
	while (dmc->counter[2] < 0)
	{
		dmc->counter[2] += dmc->dfreq;

		if ( dmc->data > 0x100 ) // data = 0x100 when shift register is empty
		{
			if (!dmc->empty)
			{
				if ((dmc->data & 1) && (dmc->damp < 63))
					dmc->damp++;
				else if (!(dmc->data & 1) && (0 < dmc->damp))
					dmc->damp--;
			}
			dmc->data >>=1;
		}

		if ( dmc->data <= 0x100 ) // shift register is empty
		{
			if (dmc->dlength > 0)
			{
				//dmc->memory->Read (dmc->daddress, dmc->data);
				dmc->data = dmc->memory[dmc->daddress];
				//cpu->StealCycles(4); // DMC read takes 3 or 4 CPU cycles, usually 4
				// (checking for the 3-cycle case would require sub-instruction emulation)
				dmc->data &= 0xFF; // read 8 bits
				if (dmc->option[OPT_DPCM_REVERSE]) dmc->data = BITREVERSE[dmc->data];
				dmc->data |= 0x10000; // use an extra bit to signal end of data
				dmc->empty = false;
				dmc->daddress = ((dmc->daddress+1)&0xFFFF)|0x8000 ;
				--dmc->dlength;
				if (dmc->dlength == 0)
				{
					if (dmc->mode & 1) // looped DPCM = auto-reload
					{
						dmc->daddress = ((dmc->adr_reg<<6)|0xC000);
						dmc->dlength = (dmc->len_reg<<4)+1;
					}
					else if (dmc->mode & 2) // IRQ and not looped
					{
						dmc->irq = true;
						//cpu->UpdateIRQ(NES_CPU::IRQD_DMC, true);
					}
				}
			}
			else
			{
				dmc->data = 0x10000; // DMC will do nothing
				dmc->empty = true;
			}
		}
	}

	return (dmc->damp<<1) + dmc->dac_lsb;
}

static void TickFrameSequence(NES_DMC* dmc, UINT32 clocks)
{
	dmc->frame_sequence_count += clocks;
	while (dmc->frame_sequence_count > dmc->frame_sequence_length)
	{
		FrameSequence(dmc, dmc->frame_sequence_step);
		dmc->frame_sequence_count -= dmc->frame_sequence_length;
		++dmc->frame_sequence_step;
		if(dmc->frame_sequence_step >= dmc->frame_sequence_steps)
			dmc->frame_sequence_step = 0;
	}
}

static void Tick(NES_DMC* dmc, UINT32 clocks)
{
	dmc->out[0] = calc_tri(dmc, clocks);
	dmc->out[1] = calc_noise(dmc, clocks);
	dmc->out[2] = calc_dmc(dmc, clocks);
}

UINT32 NES_DMC_np_Render(void* chip, INT32 b[2])
{
	NES_DMC* dmc = (NES_DMC*)chip;
	UINT32 clocks;
	INT32 m[3];

	RC_STEP(&dmc->tick_count);
	clocks = RC_GET_VAL(&dmc->tick_count);
	RC_MASK(&dmc->tick_count);
	TickFrameSequence(dmc, clocks);
	Tick(dmc, clocks);

	dmc->out[0] = (dmc->mask & 1) ? 0 : dmc->out[0];
	dmc->out[1] = (dmc->mask & 2) ? 0 : dmc->out[1];
	dmc->out[2] = (dmc->mask & 4) ? 0 : dmc->out[2];

	m[0] = dmc->tnd_table[0][dmc->out[0]][0][0];
	m[1] = dmc->tnd_table[0][0][dmc->out[1]][0];
	m[2] = dmc->tnd_table[0][0][0][dmc->out[2]];

	if (dmc->option[OPT_NONLINEAR_MIXER])
	{
		INT32 ref = m[0] + m[1] + m[2];
		INT32 voltage = dmc->tnd_table[1][dmc->out[0]][dmc->out[1]][dmc->out[2]];
		int i;
		if (ref)
		{
			for (i=0; i < 3; ++i)
				m[i] = (m[i] * voltage) / ref;
		}
		else
		{
			for (i=0; i < 3; ++i)
				m[i] = voltage;
		}
	}

	// anti-click nullifies any 4011 write but preserves nonlinearity
	if (dmc->option[OPT_DPCM_ANTI_CLICK])
	{
		if (dmc->dmc_pop) // $4011 will cause pop this frame
		{
			// adjust offset to counteract pop
			dmc->dmc_pop_offset += dmc->dmc_pop_follow - m[2];
			dmc->dmc_pop = false;

			// prevent overflow, keep headspace at edges
#define OFFSET_MAX	((1 << 30) - (4 << 16))
			if (dmc->dmc_pop_offset >  OFFSET_MAX) dmc->dmc_pop_offset =  OFFSET_MAX;
			if (dmc->dmc_pop_offset < -OFFSET_MAX) dmc->dmc_pop_offset = -OFFSET_MAX;
		}
		dmc->dmc_pop_follow = m[2]; // remember previous position

		m[2] += dmc->dmc_pop_offset; // apply offset

		// TODO implement this in a better way
		// roll off offset (not ideal, but prevents overflow)
		if (dmc->dmc_pop_offset > 0) --dmc->dmc_pop_offset;
		else if (dmc->dmc_pop_offset < 0) ++dmc->dmc_pop_offset;
	}

	b[0]  = m[0] * dmc->sm[0][0];
	b[0] += m[1] * dmc->sm[0][1];
	b[0] +=-m[2] * dmc->sm[0][2];
	b[0] >>= 7-2;

	b[1]  = m[0] * dmc->sm[1][0];
	b[1] += m[1] * dmc->sm[1][1];
	b[1] +=-m[2] * dmc->sm[1][2];
	b[1] >>= 7-2;

	return 2;
}


void NES_DMC_np_SetClock(void* chip, UINT32 c)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->clock = c;
	
	if (abs((int)dmc->clock - DEFAULT_CLK_PAL) <= 1000)	// check for approximately DEFAULT_CLK_PAL
		NES_DMC_np_SetPal(dmc, true);
	else
		NES_DMC_np_SetPal(dmc, false);
}

void NES_DMC_np_SetRate(void* chip, UINT32 r)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->rate = r ? r : DEFAULT_RATE;

	RC_SET_RATIO(&dmc->tick_count, dmc->clock, dmc->rate);
}

void NES_DMC_np_SetPal(void* chip, bool is_pal)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->pal = (is_pal ? 1 : 0);
	// set CPU cycles in frame_sequence
	dmc->frame_sequence_length = is_pal ? 8314 : 7458;
}

void NES_DMC_np_SetAPU(void* chip, void* apu_)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->apu = apu_;
}

// Initializing TRI, NOISE, DPCM mixing table
static void InitializeTNDTable(NES_DMC* dmc, double wt, double wn, double wd)
{
	// volume adjusted by 0.95 based on empirical measurements
	static const double MASTER = 8192.0 * 0.95;
	// truthfully, the nonlinear curve does not appear to match well
	// with my tests. Do more testing of the APU/DMC DAC later.
	// this value keeps the triangle consistent with measured levels,
	// but not necessarily the rest of this APU channel,
	// because of the lack of a good DAC model, currently.

	int t, n, d;

	{	// Linear Mixer
		for(t=0; t<16 ; t++) {
			for(n=0; n<16; n++) {
				for(d=0; d<128; d++) {
						dmc->tnd_table[0][t][n][d] = (UINT32)(MASTER*(3.0*t+2.0*n+d)/208.0);
				}
			}
		}
	}
	{	// Non-Linear Mixer
		dmc->tnd_table[1][0][0][0] = 0;
		for(t=0; t<16 ; t++) {
			for(n=0; n<16; n++) {
				for(d=0; d<128; d++) {
					if(t!=0||n!=0||d!=0)
						dmc->tnd_table[1][t][n][d] = (UINT32)((MASTER*159.79)/(100.0+1.0/((double)t/wt+(double)n/wn+(double)d/wd)));
				}
			}
		}
	}

}

void NES_DMC_np_Reset(void* chip)
{
	NES_DMC* dmc = (NES_DMC*)chip;
	int i;
	//dmc->mask = 0;

	InitializeTNDTable(dmc,8227,12241,22638);

	dmc->counter[0] = 0;
	dmc->counter[1] = 0;
	dmc->counter[2] = 0;
	dmc->tphase = 0;
	dmc->nfreq = wavlen_table[0][0];
	dmc->dfreq = freq_table[0][0];
	dmc->tri_freq = 0;
	dmc->linear_counter = 0;
	dmc->linear_counter_reload = 0;
	dmc->linear_counter_halt = 0;
	dmc->linear_counter_control = 0;
	dmc->noise_volume = 0;
	dmc->noise = 0;
	dmc->noise_tap = 0;
	dmc->envelope_loop = 0;
	dmc->envelope_disable = 0;
	dmc->envelope_write = 0;
	dmc->envelope_div_period = 0;
	dmc->envelope_div = 0;
	dmc->envelope_counter = 0;
	dmc->enable[0] = 0;
	dmc->enable[1] = 0;
	dmc->length_counter[0] = 0;
	dmc->length_counter[1] = 0;
	dmc->frame_irq = false;
	dmc->frame_irq_enable = false;
	dmc->frame_sequence_count = 0;
	dmc->frame_sequence_steps = 4;
	dmc->frame_sequence_step = 0;
	//cpu->UpdateIRQ(NES_CPU::IRQD_FRAME, false);

	for (i = 0; i < 0x0F; i++)
		NES_DMC_np_Write (dmc, 0x4008 + i, 0);
	NES_DMC_np_Write (dmc, 0x4017, 0x40);

	dmc->irq = false;
	NES_DMC_np_Write (dmc, 0x4015, 0x00);
	if (dmc->option[OPT_UNMUTE_ON_RESET])
		NES_DMC_np_Write (dmc, 0x4015, 0x0f);
	//cpu->UpdateIRQ(NES_CPU::IRQD_DMC, false);

	dmc->out[0] = dmc->out[1] = dmc->out[2] = 0;
	dmc->damp = 0;
	dmc->dmc_pop = false;
	dmc->dmc_pop_offset = 0;
	dmc->dmc_pop_follow = 0;
	dmc->dac_lsb = 0;
	dmc->data = 0x100;
	dmc->empty = true;
	dmc->adr_reg = 0;
	dmc->dlength = 0;
	dmc->len_reg = 0;
	dmc->daddress = 0;
	dmc->noise = 1;
	dmc->noise_tap = (1<<1);

	if (dmc->option[OPT_RANDOMIZE_NOISE])
	{
		dmc->noise |= rand();
		dmc->counter[1] = -(rand() & 511);
	}
	if (dmc->option[OPT_RANDOMIZE_TRI])
	{
		dmc->tphase = rand() & 31;
		dmc->counter[0] = -(rand() & 2047);
	}

	NES_DMC_np_SetRate(dmc, dmc->rate);
	RC_RESET(&dmc->tick_count);
}

void NES_DMC_np_SetMemory(void* chip, const UINT8* r)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	dmc->memory = r;
}

void NES_DMC_np_SetOption(void* chip, int id, int val)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	if(id<OPT_END)
	{
		dmc->option[id] = val;
		if(id==OPT_NONLINEAR_MIXER)
			InitializeTNDTable(dmc, 8227,12241,22638);
	}
}

bool NES_DMC_np_Write(void* chip, UINT16 adr, UINT8 val)
{
	static const UINT8 length_table[32] = {
		0x0A, 0xFE,
		0x14, 0x02,
		0x28, 0x04,
		0x50, 0x06,
		0xA0, 0x08,
		0x3C, 0x0A,
		0x0E, 0x0C,
		0x1A, 0x0E,
		0x0C, 0x10,
		0x18, 0x12,
		0x30, 0x14,
		0x60, 0x16,
		0xC0, 0x18,
		0x48, 0x1A,
		0x10, 0x1C,
		0x20, 0x1E
	};
	NES_DMC* dmc = (NES_DMC*)chip;

	if (adr == 0x4015)
	{
		dmc->enable[0] = (val & 4) ? true : false;
		dmc->enable[1] = (val & 8) ? true : false;

		if (!dmc->enable[0])
		{
			dmc->length_counter[0] = 0;
		}
		if (!dmc->enable[1])
		{
			dmc->length_counter[1] = 0;
		}

		if ((val & 16) && dmc->dlength == 0)
		{
			dmc->daddress = (0xC000 | (dmc->adr_reg << 6));
			dmc->dlength = (dmc->len_reg << 4) + 1;
		}
		else if (!(val & 16))
		{
			dmc->dlength = 0;
		}

		dmc->irq = false;
		//cpu->UpdateIRQ(NES_CPU::IRQD_DMC, false);

		dmc->reg[adr-0x4008] = val;
		return true;
	}

	if (adr == 0x4017)
	{
		//DEBUG_OUT("4017 = %02X\n", val);
		dmc->frame_irq_enable = ((val & 0x40) == 0x40);
		if (dmc->frame_irq_enable) dmc->frame_irq = false;
		//cpu->UpdateIRQ(NES_CPU::IRQD_FRAME, false);

		dmc->frame_sequence_count = 0;
		if (val & 0x80)
		{
			dmc->frame_sequence_steps = 5;
			dmc->frame_sequence_step = 0;
			FrameSequence(dmc, dmc->frame_sequence_step);
			++dmc->frame_sequence_step;
		}
		else
		{
			dmc->frame_sequence_steps = 4;
			dmc->frame_sequence_step = 1;
		}
	}

	if (adr<0x4008||0x4013<adr)
		return false;

	dmc->reg[adr-0x4008] = val&0xff;

	//DEBUG_OUT("$%04X %02X\n", adr, val);

	switch (adr)
	{

	// tri

	case 0x4008:
		dmc->linear_counter_control = (val >> 7) & 1;
		dmc->linear_counter_reload = val & 0x7F;
		break;

	case 0x4009:
		break;

	case 0x400a:
		dmc->tri_freq = val | (dmc->tri_freq & 0x700) ;
		break;

	case 0x400b:
		dmc->tri_freq = (dmc->tri_freq & 0xff) | ((val & 0x7) << 8) ;
		dmc->linear_counter_halt = true;
		if (dmc->enable[0])
		{
			dmc->length_counter[0] = length_table[(val >> 3) & 0x1f];
		}
		break;

	// noise

	case 0x400c:
		dmc->noise_volume = val & 15;
		dmc->envelope_div_period = val & 15;
		dmc->envelope_disable = (val >> 4) & 1;
		dmc->envelope_loop = (val >> 5) & 1;
		break;

	case 0x400d:
		break;

	case 0x400e:
		if (dmc->option[OPT_ENABLE_PNOISE])
			dmc->noise_tap = (val & 0x80) ? (1<<6) : (1<<1);
		else
			dmc->noise_tap = (1<<1);
		dmc->nfreq = wavlen_table[dmc->pal][val&15];
		break;

	case 0x400f:
		if (dmc->enable[1])
		{
			dmc->length_counter[1] = length_table[(val >> 3) & 0x1f];
		}
		dmc->envelope_write = true;
		break;

	// dmc

	case 0x4010:
		dmc->mode = (val >> 6) & 3;
		if (!(dmc->mode & 2))
		{
			dmc->irq = false;
			//cpu->UpdateIRQ(NES_CPU::IRQD_DMC, false);
		}
		dmc->dfreq = freq_table[dmc->pal][val&15];
		break;

	case 0x4011:
		if (dmc->option[OPT_ENABLE_4011])
		{
			dmc->damp = (val >> 1) & 0x3f;
			dmc->dac_lsb = val & 1;
			dmc->dmc_pop = true;
		}
		break;

	case 0x4012:
		dmc->adr_reg = val&0xff;
		// ここでdaddressは更新されない
		break;

	case 0x4013:
		dmc->len_reg = val&0xff;
		// ここでlengthは更新されない
		break;

	default:
		return false;
	}

	return true;
}

bool NES_DMC_np_Read(void* chip, UINT16 adr, UINT8* val)
{
	NES_DMC* dmc = (NES_DMC*)chip;

	if (adr == 0x4015)
	{
		*val |=(dmc->irq               ? 0x80 : 0)
			 | (dmc->frame_irq         ? 0x40 : 0)
			 | ((dmc->dlength>0)       ? 0x10 : 0)
			 | (dmc->length_counter[1] ? 0x08 : 0)
			 | (dmc->length_counter[0] ? 0x04 : 0)
			 ;

		dmc->frame_irq = false;
		//cpu->UpdateIRQ(NES_CPU::IRQD_FRAME, false);
		return true;
	}
	else if (0x4008<=adr&&adr<=0x4014)
	{
		*val |= dmc->reg[adr-0x4008];
		return true;
	}
	else
		return false;
}
