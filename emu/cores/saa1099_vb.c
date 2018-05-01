// SAA1099 sound emulation
// Valley Bell, 2018
#include <stdlib.h>
#include <string.h>	// for memset

#include <stdtype.h>
#include "../snddef.h"
#include "../RatioCntr.h"
#include "saa1099_vb.h"


typedef struct saa_frequency_generator SAA_FREQ_GEN;
typedef struct saa_noise_generator SAA_NOISE_GEN;
typedef struct saa_envelope_generator SAA_ENV_GEN;
typedef struct saa_channel SAA_CHN;
typedef struct saa_chip SAA_CHIP;

//void* saa1099v_create(UINT32 clock, UINT32 sampleRate);
//void saa1099v_destroy(void* info);
//void saa1099v_reset(void* info);
//void saa1099v_set_mute_mask(void* info, UINT32 MuteMask);

//void saa1099v_write(void* info, UINT8 offset, UINT8 data);
static void saa_write_addr(void* info, UINT8 data);
static void saa_write_data(void* info, UINT8 data);

INLINE void saa_freq_gen_reset(SAA_FREQ_GEN* fgen);
INLINE void saa_freq_gen_step(SAA_FREQ_GEN* fgen, UINT16 inc);
static void saa_envgen_load(SAA_CHIP* saa, UINT8 genID);
static void saa_envgen_step(SAA_ENV_GEN* saaEGen);
static void saa_advance(SAA_CHIP* saa, UINT32 steps);
//void saa1099v_update(void* param, UINT32 samples, DEV_SMPL** outputs);


struct saa_frequency_generator
{
	UINT16 cntr;
	UINT16 limit;
	UINT8 state;
	UINT8 trigger;
};

struct saa_noise_generator
{
	UINT8 mode;
	UINT32 state;
	SAA_FREQ_GEN fgen;
};

struct saa_envelope_generator
{
	UINT8 enable;
	UINT8 extClock;
	UINT8 step;
	UINT8 wave;
	UINT8 invert;
	
	UINT8 pos;	// position in envelope array
	UINT8 flags;
	UINT8 volL;	// envelope volume (left)
	UINT8 volR;	// envelope volume (right)
};

struct saa_channel
{
	UINT8 volL;
	UINT8 volR;
	UINT8 freq;
	UINT8 oct;
	UINT8 toneOn;
	UINT8 noiseOn;
	
	SAA_FREQ_GEN fgen;
	UINT8 state;	// tone state
	
	UINT8 muted;
};

struct saa_chip
{
	DEV_DATA _devData;
	
	SAA_CHN channels[6];
	SAA_NOISE_GEN noise[2];
	SAA_ENV_GEN env[2];
	
	UINT8 allOn;
	UINT8 curAddr;
	
	UINT8 regs[0x20];
	
	DEV_SMPL volTbl[0x10];
	
	RATIO_CNTR stepCntr;
	UINT32 sampleRate;
	UINT32 clock;
};

#define ENV_LOAD	0x80	// load buffered commands after processing this
#define ENV_STAY	0x40	// stay at this envelope value (don't loop)
static const UINT8 saa_envelopes[8][32] =
{
	{	// zero amplitude
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0|ENV_LOAD|ENV_STAY,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0|ENV_LOAD|ENV_STAY,
	},
	{	// maximum amplitude
	0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF|ENV_LOAD,
	0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF|ENV_LOAD,
	},
	{	// single decay
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0|ENV_LOAD|ENV_STAY,
	0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0|ENV_LOAD|ENV_STAY,
	},
	{	// repetitive decay
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0|ENV_LOAD,
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0|ENV_LOAD,
	},
	{	// single triangular
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0|ENV_LOAD|ENV_STAY,
	},
	{	// repetitive triangular
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0|ENV_LOAD,
	},
	{	// single attack
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF|ENV_LOAD,
	0x0|ENV_LOAD|ENV_STAY, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0|ENV_LOAD|ENV_STAY,
	},
	{	// repetitive attack
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF|ENV_LOAD,
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF|ENV_LOAD,
	}
};

void* saa1099v_create(UINT32 clock, UINT32 sampleRate)
{
	SAA_CHIP* saa;
	UINT8 curVol;
	
	saa = (SAA_CHIP*)calloc(1, sizeof(SAA_CHIP));
	if (saa == NULL)
		return NULL;
	
	saa->clock = clock;
	saa->sampleRate = sampleRate;
	RC_SET_RATIO(&saa->stepCntr, saa->clock, saa->sampleRate * 128);
	
	for (curVol = 0x00; curVol < 0x10; curVol ++)
		saa->volTbl[curVol] = curVol * 0x8000 / 16 / 6;
	
	saa1099v_set_mute_mask(saa, 0x00);
	
	return saa;
}

void saa1099v_destroy(void* info)
{
	SAA_CHIP* saa = (SAA_CHIP*)info;
	
	free(saa);
	
	return;
}

void saa1099v_reset(void* info)
{
	SAA_CHIP* saa = (SAA_CHIP*)info;
	SAA_CHN* saaCh;
	UINT8 curChn;
	
	RC_RESET(&saa->stepCntr);
	
	saa->allOn = 0x00;
	saa->curAddr = 0x00;
	memset(saa->regs, 0x00, 0x20);
	
	for (curChn = 0; curChn < 6; curChn ++)
	{
		saaCh = &saa->channels[curChn];
		
		saaCh->volL = saaCh->volR = 0x00;
		saaCh->freq = 0x00;
		saaCh->oct = 0x00;
		saaCh->toneOn = saaCh->noiseOn = 0x00;
		saa_freq_gen_reset(&saaCh->fgen);
		saaCh->state = 0x00;
		saaCh->fgen.limit = saaCh->freq ^ 0x1FF;
	}
	for (curChn = 0; curChn < 2; curChn ++)
	{
		saa_freq_gen_reset(&saa->noise[curChn].fgen);
		saa->noise[curChn].mode = 0x00;
		saa->noise[curChn].state = ~0;
		
		saa->env[curChn].enable = 0x00;
		saa->env[curChn].pos = 0x00;
		saa->env[curChn].flags = ENV_LOAD | ENV_STAY;
		saa->env[curChn].volL = saa->env[curChn].volR = 0x10;
	}
	
	return;
}

void saa1099v_set_mute_mask(void* info, UINT32 MuteMask)
{
	SAA_CHIP* saa = (SAA_CHIP*)info;
	UINT8 curChn;
	
	for (curChn = 0; curChn < 6; curChn ++)
		saa->channels[curChn].muted = (MuteMask >> curChn) & 0x01;
	
	return;
}

void saa1099v_write(void* info, UINT8 offset, UINT8 data)
{
	if (offset & 0x01)
		saa_write_addr(info, data);
	else
		saa_write_data(info, data);
}

static void saa_write_addr(void* info, UINT8 data)
{
	SAA_CHIP* saa = (SAA_CHIP*)info;
	
	saa->curAddr = data & 0x1F;
	if (saa->curAddr == 0x18 || saa->curAddr == 0x19)
	{
		SAA_ENV_GEN* saaEGen = &saa->env[saa->curAddr & 0x01];
		
		if (saaEGen->extClock)
		{
			if (saaEGen->flags & ENV_LOAD)
				saa_envgen_load(saa, saa->curAddr & 0x01);
			saa_envgen_step(saaEGen);
		}
	}
	
	return;
}

static void saa_write_data(void* info, UINT8 data)
{
	SAA_CHIP* saa = (SAA_CHIP*)info;
	UINT8 curChn;
	
	saa->regs[saa->curAddr] = data;
	switch(saa->curAddr)
	{
	case 0x00:	// channel 0..5 volume (low nibble = left, high nibble = right)
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
		curChn = saa->curAddr & 0x07;
		saa->channels[curChn].volL = (data >> 0) & 0x0F;
		saa->channels[curChn].volR = (data >> 4) & 0x0F;
		break;
	case 0x08:	// channel 0..5 frequency
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
		curChn = saa->curAddr & 0x07;
		saa->channels[curChn].freq = data;
		saa->channels[curChn].fgen.limit = data ^ 0x1FF;
		break;
	case 0x10:	// channel 0+1/2+3/4+5 octave (low nibble = 0/2/4, high nibble = 1/3/5)
	case 0x11:
	case 0x12:
		curChn = (saa->curAddr & 0x03) << 1;
		saa->channels[curChn | 0x00].oct = (data >> 0) & 0x07;
		saa->channels[curChn | 0x01].oct = (data >> 4) & 0x07;
		break;
	case 0x14:	// tone enable
		for (curChn = 0; curChn < 6; curChn ++)
			saa->channels[curChn].toneOn = (data >> curChn) & 0x01;
		break;
	case 0x15:	// noise enable
		for (curChn = 0; curChn < 6; curChn ++)
			saa->channels[curChn].noiseOn = (data >> curChn) & 0x01;
		break;
	case 0x16:	// noise generator frequencies
		saa->noise[0x00].mode = (data >> 0) & 0x03;
		saa->noise[0x01].mode = (data >> 4) & 0x03;
		for (curChn = 0; curChn < 2; curChn ++)
		{
			SAA_NOISE_GEN* saaNGen = &saa->noise[curChn];
			if (saaNGen->mode == 0x03)
				saaNGen->fgen.limit = 1;	// clocked by frequency generator
			else
				saaNGen->fgen.limit = (1 << saaNGen->mode);
			saaNGen->fgen.cntr = 0;
		}
		break;
	case 0x18:	// envelope generator parameters
	case 0x19:	// envelope generator parameters
		curChn = saa->curAddr & 0x01;
		saa->env[curChn].enable = (data >> 7) & 0x01;
		saa->env[curChn].step = (data >> 4) & 0x01;
		if (! saa->env[curChn].enable || (saa->env[curChn].flags & ENV_LOAD))
		{
			saa_envgen_load(saa, curChn);
			saa->env[curChn].pos = 0x00;
			saa_envgen_step(&saa->env[curChn]);
		}
		break;
	case 0x1C:	// chip enable / sync/reset
		saa->allOn = (data >> 0) & 0x01;
		if (data & 0x02)
		{
			// reset channel states
			for (curChn = 0; curChn < 6; curChn ++)
			{
				saa_freq_gen_reset(&saa->channels[curChn].fgen);
				saa->channels[curChn].state = 0x00;
			}
		}
		break;
	}
	
	return;
}

INLINE void saa_freq_gen_reset(SAA_FREQ_GEN* fgen)
{
	fgen->cntr = 0;
	// keep fgen->limit
	fgen->state = 0;
	fgen->trigger = 0;
	
	return;
}

INLINE void saa_freq_gen_step(SAA_FREQ_GEN* fgen, UINT16 inc)
{
	fgen->cntr += inc;
	if (fgen->cntr < fgen->limit)
	{
		fgen->trigger = 0;
	}
	else
	{
		fgen->cntr -= fgen->limit;
		fgen->state ^= 1;
		fgen->trigger = 1;
	}
	
	return;
}

#define FREQGEN_FALL_EDGE(fgen)	( (fgen).trigger && ! (fgen).state )
static void saa_advance(SAA_CHIP* saa, UINT32 steps)
{
	SAA_CHN* saaCh;
	SAA_NOISE_GEN* saaNGen;
	SAA_ENV_GEN* saaEGen;
	UINT8 curChn;
	
	if (! steps)
		return;
	
	for (; steps > 0; steps --)
	{
		// run frequency generators, process tone channels
		for (curChn = 0; curChn < 6; curChn ++)
		{
			saaCh = &saa->channels[curChn];
			
			saa_freq_gen_step(&saaCh->fgen, 1 << saaCh->oct);
			if (FREQGEN_FALL_EDGE(saaCh->fgen))
				saaCh->state ^= 1;
		}
		
		// run noise generators, process noise channels
		for (curChn = 0; curChn < 2; curChn ++)
		{
			UINT16 inc;
			
			saaNGen = &saa->noise[curChn];
			
			// modes 0..2: // master clock, divided by 128 / 256 / 512
			// mode 3: using frequency generator 0/3
			if (saaNGen->mode == 0x03)
				inc = FREQGEN_FALL_EDGE(saa->channels[curChn * 3 + 0].fgen);
			else
				inc = 1;
			saa_freq_gen_step(&saaNGen->fgen, inc);
			if (saaNGen->fgen.trigger)
			{
				// Polynomial of noise generator: x^18 + x^11 + x (i.e. 0x20400)
				// see http://www.vogons.org/viewtopic.php?f=9&t=51695
				if ( (! (saaNGen->state & 0x20000)) != (! (saaNGen->state & 0x00400)) )
					saaNGen->state = (saaNGen->state << 1) | 0x01;
				else
					saaNGen->state <<= 1;
			}
		}
		
		// run envelope generators
		for (curChn = 0; curChn < 2; curChn ++)
		{
			saaEGen = &saa->env[curChn];
			
			if (! saaEGen->extClock && FREQGEN_FALL_EDGE(saa->channels[curChn * 3 + 1].fgen))
			{
				if (saaEGen->flags & ENV_LOAD)
					saa_envgen_load(saa, curChn);
				saa_envgen_step(saaEGen);
			}
		}
	}
	
	return;
}

static void saa_envgen_load(SAA_CHIP* saa, UINT8 genID)
{
	SAA_ENV_GEN* saaEGen = &saa->env[genID];
	UINT8 data = saa->regs[0x18 | genID];
	
	saaEGen->extClock = (data >> 5) & 0x01;
	saaEGen->wave = (data >> 1) & 0x07;
	saaEGen->invert = (data >> 0) & 0x01;
	
	if (! saaEGen->enable)
	{
		saaEGen->pos = 0x00;
		saaEGen->flags = ENV_LOAD | ENV_STAY;
		saaEGen->volL = saaEGen->volR = 0x10;
	}
	
	return;
}

static void saa_envgen_step(SAA_ENV_GEN* saaEGen)
{
	UINT8 wdata;
	
	if (! saaEGen->enable)
		return;
	
	wdata = saa_envelopes[saaEGen->wave][saaEGen->pos];
	saaEGen->flags = wdata & 0xF0;
	saaEGen->volL = saaEGen->volR = wdata & 0x0F;
	if (saaEGen->invert)
		saaEGen->volR ^= 0x0F;
	
	if (! (saaEGen->flags & ENV_STAY))
	{
		saaEGen->pos ++;
		saaEGen->pos &= 0x1F;
	}
	if (saaEGen->step)
	{
		saaEGen->flags |= (saa_envelopes[saaEGen->wave][saaEGen->pos] & 0xF0);
		if (! (saaEGen->flags & ENV_STAY))
		{
			saaEGen->pos ++;
			saaEGen->pos &= 0x1F;
		}
	}
	
	return;
}

void saa1099v_update(void* param, UINT32 samples, DEV_SMPL** outputs)
{
	SAA_CHIP* saa = (SAA_CHIP*)param;
	SAA_CHN* saaCh;
	UINT32 curSmpl;
	UINT8 curChn;
	UINT8 neChn;	// noise/envelope channel
	DEV_SMPL outState;
	DEV_SMPL outVolL;
	DEV_SMPL outVolR;
	
	memset(outputs[0], 0, samples * sizeof(DEV_SMPL));
	memset(outputs[1], 0, samples * sizeof(DEV_SMPL));
	if (! saa->allOn)
		return;
	
	for (curSmpl = 0; curSmpl < samples; curSmpl ++)
	{
		RC_STEP(&saa->stepCntr);
		saa_advance(saa, RC_GET_VAL(&saa->stepCntr));
		RC_MASK(&saa->stepCntr);
		
		for (curChn = 0; curChn < 6; curChn ++)
		{
			saaCh = &saa->channels[curChn];
			if (saaCh->muted)
				continue;
			neChn = curChn / 3;
			
			outState = 0;
			if (saaCh->toneOn)
				outState += saaCh->state ? +1 : -1;
			if (saaCh->noiseOn)
				outState += (saa->noise[neChn].state & 0x01) ? +1 : -1;
			
			if ((curChn == 2 || curChn == 5) && saa->env[neChn].enable)
			{
				// enabling the envelope generator disables amplitude bit 0
				outVolL = (saa->volTbl[saaCh->volL & ~0x01] * saa->env[neChn].volL) >> 4;
				outVolR = (saa->volTbl[saaCh->volR & ~0x01] * saa->env[neChn].volR) >> 4;
			}
			else
			{
				outVolL = saa->volTbl[saaCh->volL];
				outVolR = saa->volTbl[saaCh->volR];
			}
			
			outputs[0][curSmpl] += outState * outVolL;
			outputs[1][curSmpl] += outState * outVolR;
		}
	}
	
	return;
}
