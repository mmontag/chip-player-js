/*-----------------------------------------------------------------------------

	ST-Sound ( YM files player library )

	Copyright (C) 1995-1999 Arnaud Carre ( http://leonard.oxg.free.fr )

	Extended YM-2149 Emulator, with ATARI music demos effects.
	(SID-Like, Digidrum, Sync Buzzer, Sinus SID and Pattern SID)

-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------

	This file is part of ST-Sound

	ST-Sound is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	ST-Sound is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with ST-Sound; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

-----------------------------------------------------------------------------*/

/* Modified for xmp by Claudio Matsuoka, 30-Aug-2011 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "ym2149.h"

//-------------------------------------------------------------------
// env shapes.
//-------------------------------------------------------------------
static const int Env00xx[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
static const int Env01xx[8] = { 0, 1, 0, 0, 0, 0, 0, 0 };
static const int Env1000[8] = { 1, 0, 1, 0, 1, 0, 1, 0 };
static const int Env1001[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
static const int Env1010[8] = { 1, 0, 0, 1, 1, 0, 0, 1 };
static const int Env1011[8] = { 1, 0, 1, 1, 1, 1, 1, 1 };
static const int Env1100[8] = { 0, 1, 0, 1, 0, 1, 0, 1 };
static const int Env1101[8] = { 0, 1, 1, 1, 1, 1, 1, 1 };
static const int Env1110[8] = { 0, 1, 1, 0, 0, 1, 1, 0 };
static const int Env1111[8] = { 0, 1, 0, 0, 0, 0, 0, 0 };

static const int *EnvWave[16] = {
	Env00xx, Env00xx, Env00xx, Env00xx, Env01xx, Env01xx, Env01xx, Env01xx,
	Env1000, Env1001, Env1010, Env1011, Env1100, Env1101, Env1110, Env1111
};

static int ymVolumeTable[16] = {
	62, 161, 265, 377, 580, 774, 1155, 1575, 2260, 3088, 4570, 6233, 9330,
	13187, 21220, 32767
};

//----------------------------------------------------------------------
// Very cool and fast DC Adjuster ! This is the *new* stuff of that
// package coz I get that idea working on SainT in 2004 !
// ( almost everything here is from 1995 !)
//----------------------------------------------------------------------
static void dc_adjuster_reset(struct dc_adjuster *dc)
{
	memset(dc->m_buffer, 0, DC_ADJUST_BUFFERLEN);
	dc->m_pos = 0;
	dc->m_sum = 0;
}

struct dc_adjuster *dc_adjuster_new()
{
	struct dc_adjuster *dc;

	dc = malloc(sizeof (struct dc_adjuster));
	if (dc == NULL)
		return dc;
	dc_adjuster_reset(dc);

	return dc;
}

static int dc_adjuster_get_dc_level(struct dc_adjuster *dc)
{
	return dc->m_sum / DC_ADJUST_BUFFERLEN;
}

void dc_adjuster_addsample(struct dc_adjuster *dc, int sample)
{
	dc->m_sum -= dc->m_buffer[dc->m_pos];
	dc->m_sum += sample;
	dc->m_buffer[dc->m_pos] = sample;
	dc->m_pos = (dc->m_pos + 1) & (DC_ADJUST_BUFFERLEN - 1);
}

void dc_adjuster_destroy(struct dc_adjuster *dc)
{
	free(dc);
}

//----------------------------------------------------------------------
// Very simple low pass filter.
// Filter coefs are 0.25,0.5,0.25
//----------------------------------------------------------------------
inline ymsample *getBufferCopy(struct ym2149 *ym, ymsample *in, int len)
{
	if (len > ym->internalInputLen) {
		ym->internalInput = (ymsample *)malloc(len * sizeof(ymsample));
		ym->internalInputLen = len;
	}
	memcpy(ym->internalInput, in, len * sizeof(ymsample));
	return ym->internalInput;
}

#define	DSP_FILTER(a,b,c)	(((int)(a)+((int)b+(int)b)+(int)(c))>>2)

/* Cheap but efficient low pass filter ( 0.25,0.5,0.25 )
 * filter: out -> out
 */
void lowpFilterProcess(struct ym2149 *ym, ymsample *out, int len)
{
	ymsample *in;
	int i;

	in = getBufferCopy(ym, out, len);

	if (len > 0) {
		*out++ = DSP_FILTER(ym->oldFilter[0], ym->oldFilter[1], in[0]);

		if (len > 1)
			*out++ = DSP_FILTER(ym->oldFilter[1], in[0], in[1]);
	}

	ym->oldFilter[0] = in[len - 2];
	ym->oldFilter[1] = in[len - 1];

	for (i = 2; i < len; i++) {
		*out++ = DSP_FILTER(in[0], in[1], in[2]);
		in++;
	}
}


static uint8 *ym2149EnvInit(uint8 *pEnv, int a, int b)
{
	int i;
	int d;

	d = b - a;
	a *= 15;
	for (i = 0; i < 16; i++) {
		*pEnv++ = (uint8)a;
		a += d;
	}
	return pEnv;
}

static uint32 toneStepCompute(struct ym2149 *ym, int rHigh, int rLow)
{
	int per = rHigh & 15;

	per = (per << 8) + rLow;
	if (per <= 5)
		return 0;

#ifdef YM_INTEGER_ONLY
	int64 step = ym->internalClock;
	step <<= (15 + 16 - 3);
	step /= (per * ym->replayFrequency);
#else
	float step = ym->internalClock;
	step /= ((float) per * 8.0 * (float)ym->replayFrequency);
	step *= 32768.0 * 65536.0;
#endif

	return (uint32)step;
}

static uint32 noiseStepCompute(struct ym2149 *ym, int rNoise)
{
	int per = (rNoise & 0x1f);
	if (per < 3)
		return 0;

#ifdef YM_INTEGER_ONLY
	int64 step = ym->internalClock;
	step <<= (16 - 1 - 3);
	step /= (per * ym->replayFrequency);
#else
	float step = ym->internalClock;
	step /= ((float) per * 8.0 * (float)ym->replayFrequency);
	step *= 65536.0 / 2.0;
#endif

	return (uint32)step;
}

static uint32 rndCompute(struct ym2149 *ym)
{
	int rBit = (ym->rndRack & 1) ^ ((ym->rndRack >> 2) & 1);
	ym->rndRack = (ym->rndRack >> 1) | (rBit << 16);
	return rBit ? 0 : 0xffff;
}

static uint32 envStepCompute(struct ym2149 *ym, int rHigh, int rLow)
{
	int per = rHigh;

	per = (per << 8) + rLow;
	if (per < 3)
		return 0;

#ifdef YM_INTEGER_ONLY
	int64 step = ym->internalClock;
	step <<= (16 + 16 - 9);
	step /= (per * ym->replayFrequency);
#else
	float step = ym->internalClock;
	step /= ((float) per * 512.0 * (float)ym->replayFrequency);
	step *= 65536.0 * 65536.0;
#endif

	return (uint32)step;
}

static ymsample nextSample(struct ym2149 *ym)
{
	int vol;
	int bt, bn;

	if (ym->noisePos & 0xffff0000) {
		ym->currentNoise ^= rndCompute(ym);
		ym->noisePos &= 0xffff;
	}
	bn = ym->currentNoise;
	ym->volE = ymVolumeTable[ym->envData[ym->envShape][ym->envPhase][ym->envPos >> (32 - 5)]];

	/*---------------------------------------------------
	 * Tone+noise+env+DAC for three voices !
	 *--------------------------------------------------- */
	bt = ((((int32)ym->posA) >> 31) | ym->mixerTA) & (bn | ym->mixerNA);
	vol = (*ym->pVolA) & bt;
	bt = ((((int32)ym->posB) >> 31) | ym->mixerTB) & (bn | ym->mixerNB);
	vol += (*ym->pVolB) & bt;
	bt = ((((int32)ym->posC) >> 31) | ym->mixerTC) & (bn | ym->mixerNC);
	vol += (*ym->pVolC) & bt;

	/*---------------------------------------------------
	 * Inc
	 *--------------------------------------------------- */
	ym->posA += ym->stepA;
	ym->posB += ym->stepB;
	ym->posC += ym->stepC;
	ym->noisePos += ym->noiseStep;
	ym->envPos += ym->envStep;

	if (0 == ym->envPhase) {
		if (ym->envPos < ym->envStep) {
			ym->envPhase = 1;
		}
	}

return vol;
	/*---------------------------------------------------
	 * Normalize process
	 *--------------------------------------------------- */
	dc_adjuster_addsample(ym->dc, vol);

	return (vol - dc_adjuster_get_dc_level(ym->dc));
}

struct ym2149 *ym2149_new(int masterClock, int prediv, int playRate)
{
	int /*i,*/ env;
	uint8 *pEnv;
	struct ym2149 *ym;

	ym = calloc(1, sizeof (struct ym2149));
	if (ym == NULL)
		goto err1;
	
	ym->dc = dc_adjuster_new();
	if (ym->dc == NULL)
		goto err2;

	ym->frameCycle = 0;

#if 0
	if (ymVolumeTable[15] == 32767)	/* excuse me for that bad trick ;-) */
	{
		for (i = 0; i < 16; i++) {
			ymVolumeTable[i] = (ymVolumeTable[i] * 2) / 6;
		}
	}
#endif

	/*--------------------------------------------------------
	 * build env shapes.
	 *-------------------------------------------------------- */
	pEnv = &ym->envData[0][0][0];
	for (env = 0; env < 16; env++) {
		const int *pse = EnvWave[env];
		int phase;

		for (phase = 0; phase < 4; phase++) {
			pEnv = ym2149EnvInit(pEnv, pse[phase * 2 + 0],
						  pse[phase * 2 + 1]);
		}
	}

	ym->internalClock = masterClock / prediv; /* YM at 2Mhz on ATARI ST */
	ym->replayFrequency = playRate;		/* DAC at 44.1Khz on PC */
	ym->cycleSample = 0;

	/* Set volume voice pointers. */
	ym->pVolA = &ym->volA;
	ym->pVolB = &ym->volB;
	ym->pVolC = &ym->volC;

	/* Reset YM2149 */
	ym2149_reset(ym);

	return ym;

err2:
	free(ym);
err1:
	return NULL;
}

void ym2149_destroy(struct ym2149 *ym)
{
	dc_adjuster_destroy(ym->dc);
	free(ym->internalInput);
	free(ym);
}

int ym2149_read_register(struct ym2149 *ym, int reg)
{
	if ((reg >= 0) && (reg <= 13))
		return ym->regs[reg];
	else
		return -1;
}

void ym2149_write_register(struct ym2149 *ym, int reg, int data)
{
 	/* Assume output always 1 if 0 period (for Digi-sample !) */

	switch (reg) {
	case 0:
		ym->regs[0] = data & 255;
		ym->stepA = toneStepCompute(ym, ym->regs[1], ym->regs[0]);
		if (!ym->stepA)
			ym->posA = (1 << 31);
		break;
	case 2:
		ym->regs[2] = data & 255;
		ym->stepB = toneStepCompute(ym, ym->regs[3], ym->regs[2]);
		if (!ym->stepB)
			ym->posB = (1 << 31);
		break;
	case 4:
		ym->regs[4] = data & 255;
		ym->stepC = toneStepCompute(ym, ym->regs[5], ym->regs[4]);
		if (!ym->stepC)
			ym->posC = (1 << 31);
		break;
	case 1:
		ym->regs[1] = data & 15;
		ym->stepA = toneStepCompute(ym, ym->regs[1], ym->regs[0]);
		if (!ym->stepA)
			ym->posA = (1 << 31);
		break;
	case 3:
		ym->regs[3] = data & 15;
		ym->stepB = toneStepCompute(ym, ym->regs[3], ym->regs[2]);
		if (!ym->stepB)
			ym->posB = (1 << 31);
		break;
	case 5:
		ym->regs[5] = data & 15;
		ym->stepC = toneStepCompute(ym, ym->regs[5], ym->regs[4]);
		if (!ym->stepC)
			ym->posC = (1 << 31);
		break;
	case 6:
		ym->regs[6] = data & 0x1f;
		ym->noiseStep = noiseStepCompute(ym, ym->regs[6]);
		if (!ym->noiseStep) {
			ym->noisePos = 0;
			ym->currentNoise = 0xffff;
		}
		break;
	case 7:
		ym->regs[7] = data & 255;
		ym->mixerTA = (data & (1 << 0)) ? 0xffff : 0;
		ym->mixerTB = (data & (1 << 1)) ? 0xffff : 0;
		ym->mixerTC = (data & (1 << 2)) ? 0xffff : 0;
		ym->mixerNA = (data & (1 << 3)) ? 0xffff : 0;
		ym->mixerNB = (data & (1 << 4)) ? 0xffff : 0;
		ym->mixerNC = (data & (1 << 5)) ? 0xffff : 0;
		break;
	case 8:
		ym->regs[8] = data & 31;
		ym->volA = ymVolumeTable[data & 15];
		if (data & 0x10)
			ym->pVolA = &ym->volE;
		else
			ym->pVolA = &ym->volA;
		break;
	case 9:
		ym->regs[9] = data & 31;
		ym->volB = ymVolumeTable[data & 15];
		if (data & 0x10)
			ym->pVolB = &ym->volE;
		else
			ym->pVolB = &ym->volB;
		break;
	case 10:
		ym->regs[10] = data & 31;
		ym->volC = ymVolumeTable[data & 15];
		if (data & 0x10)
			ym->pVolC = &ym->volE;
		else
			ym->pVolC = &ym->volC;
		break;
	case 11:
		ym->regs[11] = data & 255;
		ym->envStep = envStepCompute(ym, ym->regs[12], ym->regs[11]);
		break;
	case 12:
		ym->regs[12] = data & 255;
		ym->envStep = envStepCompute(ym, ym->regs[12], ym->regs[11]);
		break;
	case 13:
		ym->regs[13] = data & 0xf;
		ym->envPos = 0;
		ym->envPhase = 0;
		ym->envShape = data & 0xf;
		break;
	}
}

void ym2149_update(struct ym2149 *ym, ymsample *pSampleBuffer, int nbSample, int vl, int vr, int stereo)
{
	ymsample *pBuffer = pSampleBuffer;
	int nbs = nbSample;

	for (; nbSample > 0; nbSample--) {
		ymsample sample = nextSample(ym);

		if (stereo)
			*pSampleBuffer++ = sample * vr;
		*pSampleBuffer++ = sample * vl;
	}

	lowpFilterProcess(ym, (ymsample *)pBuffer, nbs);
}

void ym2149_reset(struct ym2149 *ym)
{
	ym2149_write_register(ym, 7, 0x3f);
	ym2149_write_register(ym, 8, 0);
	ym2149_write_register(ym, 9, 0);
	ym2149_write_register(ym, 10, 0);
	ym->currentNoise = 0xffff;
	ym->rndRack = 1;
	ym->envShape = 0;
	ym->envPhase = 0;
	ym->envPos = 0;
	dc_adjuster_reset(ym->dc);
}
