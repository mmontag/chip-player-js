//
// NES 2A03
//
// Ported from NSFPlay 2.2 to VGMPlay (including C++ -> C conversion)
// by Valley Bell on 24 September 2013
// Updated to NSFPlay 2.3 on 26 September 2013
// (Note: Encoding is UTF-8)

#include <stdlib.h>
#include <stddef.h>	// for NULL

#include "../../stdtype.h"
#include "../../stdbool.h"
#include "../snddef.h"
#include "../RatioCntr.h"
#include "np_nes_apu.h"


// Master Clock: 21477272 (NTSC)
// APU Clock = Master Clock / 12
#define DEFAULT_CLOCK	1789772.0	// not sure if this shouldn't be 1789772.667 instead
#define DEFAULT_RATE	44100


/** Upper half of APU **/
enum
{
	OPT_UNMUTE_ON_RESET=0,
	OPT_NONLINEAR_MIXER,
	OPT_PHASE_REFRESH,
	OPT_DUTY_SWAP,
	OPT_NEGATE_SWEEP_INIT,
	OPT_END
};

enum
{
	SQR0_MASK = 1,
	SQR1_MASK = 2,
};


typedef struct _NES_APU NES_APU;
struct _NES_APU
{
	DEV_DATA _devData;

	int option[OPT_END];		// 各種オプション
	int mask;
	INT32 sm[2][2];

	UINT32 gclock;
	UINT8 reg[0x20];
	INT32 out[2];
	UINT32 rate, clock;

	INT32 square_table[32];		// nonlinear mixer
	INT32 square_linear;		// linear mix approximation

	int scounter[2];			// frequency divider
	int sphase[2];				// phase counter

	int duty[2];
	int volume[2];
	int freq[2];
	int sfreq[2];

	bool sweep_enable[2];
	bool sweep_mode[2];
	bool sweep_write[2];
	int sweep_div_period[2];
	int sweep_div[2];
	int sweep_amount[2];

	bool envelope_disable[2];
	bool envelope_loop[2];
	bool envelope_write[2];
	int envelope_div_period[2];
	int envelope_div[2];
	int envelope_counter[2];

	int length_counter[2];

	bool enable[2];

	RATIO_CNTR tick_count;
};

static void sweep_sqr(NES_APU* apu, int ch);	// calculates target sweep frequency
static INT32 calc_sqr(NES_APU* apu, int ch, UINT32 clocks);
static void Tick(NES_APU* apu, UINT32 clocks);


static void sweep_sqr(NES_APU* apu, int i)
{
	int shifted = apu->freq[i] >> apu->sweep_amount[i];
	if (i == 0 && apu->sweep_mode[i]) shifted += 1;
	apu->sfreq[i] = apu->freq[i] + (apu->sweep_mode[i] ? -shifted : shifted);
	//DEBUG_OUT("shifted[%d] = %d (%d >> %d)\n",i,shifted,apu->freq[i],apu->sweep_amount[i]);
}

void NES_APU_np_FrameSequence(void* chip, int s)
{
	NES_APU* apu = (NES_APU*)chip;
	int i;

	//DEBUG_OUT("FrameSequence(%d)\n",s);

	if (s > 3) return; // no operation in step 4

	// 240hz clock
	for (i=0; i < 2; ++i)
	{
		bool divider = false;
		if (apu->envelope_write[i])
		{
			apu->envelope_write[i] = false;
			apu->envelope_counter[i] = 15;
			apu->envelope_div[i] = 0;
		}
		else
		{
			++apu->envelope_div[i];
			if (apu->envelope_div[i] > apu->envelope_div_period[i])
			{
				divider = true;
				apu->envelope_div[i] = 0;
			}
		}
		if (divider)
		{
			if (apu->envelope_loop[i] && apu->envelope_counter[i] == 0)
				apu->envelope_counter[i] = 15;
			else if (apu->envelope_counter[i] > 0)
				--apu->envelope_counter[i];
		}
	}

	// 120hz clock
	if ((s&1) == 0)
	  for (i=0; i < 2; ++i)
	  {
		if (!apu->envelope_loop[i] && (apu->length_counter[i] > 0))
			--apu->length_counter[i];

		if (apu->sweep_enable[i])
		{
			//DEBUG_OUT("Clock sweep: %d\n", i);

			--apu->sweep_div[i];
			if (apu->sweep_div[i] <= 0)
			{
				sweep_sqr(apu, i);	// calculate new sweep target

				//DEBUG_OUT("sweep_div[%d] (0/%d)\n",i,apu->sweep_div_period[i]);
				//DEBUG_OUT("freq[%d]=%d > sfreq[%d]=%d\n",i,apu->freq[i],i,apu->sfreq[i]);

				if (apu->freq[i] >= 8 && apu->sfreq[i] < 0x800 && apu->sweep_amount[i] > 0) // update frequency if appropriate
				{
					apu->freq[i] = apu->sfreq[i] < 0 ? 0 : apu->sfreq[i];
				}
				apu->sweep_div[i] = apu->sweep_div_period[i] + 1;

				//DEBUG_OUT("freq[%d]=%d\n",i,apu->freq[i]);
			}

			if (apu->sweep_write[i])
			{
				apu->sweep_div[i] = apu->sweep_div_period[i] + 1;
				apu->sweep_write[i] = false;
			}
		}
	  }

}

static INT32 calc_sqr(NES_APU* apu, int i, UINT32 clocks)
{
	static const INT16 sqrtbl[4][16] = {
		{0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
		{1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
	};
	INT32 ret;

	apu->scounter[i] -= clocks;
	while (apu->scounter[i] < 0)
	{
		apu->sphase[i] = (apu->sphase[i] + 1) & 15;
		apu->scounter[i] += apu->freq[i] + 1;
	}

	ret = 0;
	if (apu->length_counter[i] > 0 &&
		apu->freq[i] >= 8 &&
		apu->sfreq[i] < 0x800
		)
	{
		int v = apu->envelope_disable[i] ? apu->volume[i] : apu->envelope_counter[i];
		ret = sqrtbl[apu->duty[i]][apu->sphase[i]] ? v : 0;
	}
	
	return ret;
}

bool NES_APU_np_Read(void* chip, UINT16 adr, UINT8* val)
{
	NES_APU* apu = (NES_APU*)chip;

	if (0x4000 <= adr && adr < 0x4008)
	{
		*val |= apu->reg[adr&0x7];
		return true;
	}
	else if(adr==0x4015)
	{
		*val |= (apu->length_counter[1]?2:0)|(apu->length_counter[0]?1:0);
		return true;
	}
	else
		return false;
}

static void Tick(NES_APU* apu, UINT32 clocks)
{
	apu->out[0] = calc_sqr(apu, 0, clocks);
	apu->out[1] = calc_sqr(apu, 1, clocks);
}

// 生成される波形の振幅は0-8191
UINT32 NES_APU_np_Render(void* chip, INT32 b[2])
{
	NES_APU* apu = (NES_APU*)chip;
	UINT32 clocks;
	INT32 m[2];

	RC_STEP(&apu->tick_count);
	clocks = RC_GET_VAL(&apu->tick_count);
	RC_MASK(&apu->tick_count);
	Tick(apu, clocks);

	apu->out[0] = (apu->mask & 1) ? 0 : apu->out[0];
	apu->out[1] = (apu->mask & 2) ? 0 : apu->out[1];

	if(apu->option[OPT_NONLINEAR_MIXER])
	{
		INT32 ref;
		INT32 voltage = apu->square_table[apu->out[0] + apu->out[1]];
		m[0] = apu->out[0] << 6;
		m[1] = apu->out[1] << 6;
		ref = m[0] + m[1];
		if (ref > 0)
		{
			m[0] = (m[0] * voltage) / ref;
			m[1] = (m[1] * voltage) / ref;
		}
		else
		{
			m[0] = voltage;
			m[1] = voltage;
		}
	}
	else
	{
		m[0] = (apu->out[0] * apu->square_linear) / 15;
		m[1] = (apu->out[1] * apu->square_linear) / 15;
	}

	// Shifting is (x-2) to match the volume of MAME's NES APU sound core
	b[0]  = m[0] * apu->sm[0][0];
	b[0] += m[1] * apu->sm[0][1];
	b[0] >>= 7-2;	// was 7, but is now 8 for bipolar square

	b[1]  = m[0] * apu->sm[1][0];
	b[1] += m[1] * apu->sm[1][1];
	b[1] >>= 7-2;	// see above

	return 2;
}

void* NES_APU_np_Create(UINT32 clock, UINT32 rate)
{
	NES_APU* apu;
	int i, c, t;

	apu = (NES_APU*)calloc(1, sizeof(NES_APU));
	if (apu == NULL)
		return NULL;

	NES_APU_np_SetClock(apu, clock);
	NES_APU_np_SetRate(apu, rate);
	apu->option[OPT_UNMUTE_ON_RESET] = true;
	apu->option[OPT_PHASE_REFRESH] = true;
	apu->option[OPT_NONLINEAR_MIXER] = true;
	apu->option[OPT_DUTY_SWAP] = false;
	apu->option[OPT_NEGATE_SWEEP_INIT] = false;

	apu->square_table[0] = 0;
	for(i=1;i<32;i++)
		apu->square_table[i]=(INT32)((8192.0*95.88)/(8128.0/i+100));

	apu->square_linear = apu->square_table[15]; // match linear scale to one full volume square of nonlinear

	for(c=0;c<2;++c)
		for(t=0;t<2;++t)
			apu->sm[c][t] = 128;

	apu->mask = 0;

	return apu;
}

void NES_APU_np_Destroy(void* chip)
{
	free(chip);
}

void NES_APU_np_Reset(void* chip)
{
	NES_APU* apu = (NES_APU*)chip;
	int i;
	apu->gclock = 0;
	//apu->mask = 0;

	for (i=0; i<2; ++i)
	{
		apu->scounter[i] = 0;
		apu->sphase[i] = 0;
		apu->duty[i] = 0;
		apu->volume[i] = 0;
		apu->freq[i] = 0;
		apu->sfreq[i] = 0;
		apu->sweep_enable[i] = 0;
		apu->sweep_mode[i] = 0;
		apu->sweep_write[i] = 0;
		apu->sweep_div_period[i] = 0;
		apu->sweep_div[i] = 1;
		apu->sweep_amount[i] = 0;
		apu->envelope_disable[i] = 0;
		apu->envelope_loop[i] = 0;
		apu->envelope_write[i] = 0;
		apu->envelope_div_period[i] = 0;
		apu->envelope_div[i] = 0;
		apu->envelope_counter[i] = 0;
		apu->length_counter[i] = 0;
		apu->enable[i] = 0;
	}

	for (i = 0x4000; i < 0x4008; i++)
		NES_APU_np_Write(apu, i, 0);

	NES_APU_np_Write(apu, 0x4015, 0);
	if (apu->option[OPT_UNMUTE_ON_RESET])
		NES_APU_np_Write(apu, 0x4015, 0x0f);
	if (apu->option[OPT_NEGATE_SWEEP_INIT])
	{
		NES_APU_np_Write(apu, 0x4001, 0x08);
		NES_APU_np_Write(apu, 0x4005, 0x08);
	}

	for (i = 0; i < 2; i++)
		apu->out[i] = 0;

	NES_APU_np_SetRate(apu, apu->rate);
	RC_RESET(&apu->tick_count);
}

void NES_APU_np_SetOption(void* chip, int id, int val)
{
	NES_APU* apu = (NES_APU*)chip;

	if(id<OPT_END) apu->option[id] = val;
}

void NES_APU_np_SetClock(void* chip, UINT32 c)
{
	NES_APU* apu = (NES_APU*)chip;

	apu->clock = c;
}

void NES_APU_np_SetRate(void* chip, UINT32 r)
{
	NES_APU* apu = (NES_APU*)chip;

	apu->rate = r ? r : DEFAULT_RATE;

	RC_SET_RATIO(&apu->tick_count, apu->clock, apu->rate);
}

void NES_APU_np_SetMask(void* chip, int m)
{
	NES_APU* apu = (NES_APU*)chip;
	apu->mask = m;
}

void NES_APU_np_SetStereoMix(void* chip, int trk, INT16 mixl, INT16 mixr)
{
	NES_APU* apu = (NES_APU*)chip;

	if (trk < 0) return;
	if (trk > 1) return;
	apu->sm[0][trk] = mixl;
	apu->sm[1][trk] = mixr;
}

bool NES_APU_np_Write(void* chip, UINT16 adr, UINT8 val)
{
	NES_APU* apu = (NES_APU*)chip;
	int ch;

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

	if (0x4000 <= adr && adr < 0x4008)
	{
		//DEBUG_OUT("$%04X = %02X\n",adr,val);

		adr &= 0xf;
		ch = adr >> 2;
		switch (adr)
		{
		case 0x0:
		case 0x4:
			apu->volume[ch] = val & 15;
			apu->envelope_disable[ch] = (val >> 4) & 1;
			apu->envelope_loop[ch] = (val >> 5) & 1;
			apu->envelope_div_period[ch] = (val & 15);
			apu->duty[ch] = (val >> 6) & 3;
			if (apu->option[OPT_DUTY_SWAP])
			{
				if      (apu->duty[ch] == 1) apu->duty[ch] = 2;
				else if (apu->duty[ch] == 2) apu->duty[ch] = 1;
			}
			break;

		case 0x1:
		case 0x5:
			apu->sweep_enable[ch] = (val >> 7) & 1;
			apu->sweep_div_period[ch] = (((val >> 4) & 7));
			apu->sweep_mode[ch] = (val >> 3) & 1;
			apu->sweep_amount[ch] = val & 7;
			apu->sweep_write[ch] = true;
			sweep_sqr(apu, ch);
			break;

		case 0x2:
		case 0x6:
			apu->freq[ch] = val | (apu->freq[ch] & 0x700) ;
			sweep_sqr(apu, ch);
			break;

		case 0x3:
		case 0x7:
			apu->freq[ch] = (apu->freq[ch] & 0xFF) | ((val & 0x7) << 8) ;
			if (apu->option[OPT_PHASE_REFRESH])
				apu->sphase[ch] = 0;
			apu->envelope_write[ch] = true;
			if (apu->enable[ch])
			{
				apu->length_counter[ch] = length_table[(val >> 3) & 0x1f];
			}
			sweep_sqr(apu, ch);
			break;

		default:
			return false;
		}
		apu->reg[adr] = val;
		return true;
	}
	else if (adr == 0x4015)
	{
		apu->enable[0] = (val & 1) ? true : false;
		apu->enable[1] = (val & 2) ? true : false;

		if (!apu->enable[0])
			apu->length_counter[0] = 0;
		if (!apu->enable[1])
			apu->length_counter[1] = 0;

		apu->reg[adr-0x4000] = val;
		return true;
	}

	// 4017 is handled in np_nes_dmc.c
	//else if (adr == 0x4017)
	//{
	//}

	return false;
}
