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

#ifndef __YM2149_H
#define __YM2149_H

#define	AMSTRAD_CLOCK	1000000L
#define	ATARI_CLOCK	2000000L
#define	SPECTRUM_CLOCK	1773400L
#define	MFP_CLOCK	2457600L
#define	NOISESIZE	16384
#define	MAX_NBSAMPLE	1024
#define DC_ADJUST_BUFFERLEN 512

#define VOICE_A 0
#define VOICE_B 1
#define VOICE_C 2

/* Nice macros from SC68 */
#define YM_BASEPERL	0  /* YM-2149 LSB period base (canal A) */
#define YM_BASEPERH	1  /* YM-2149 MSB period base (canal A) */
#define YM_BASEVOL	8  /* YM-2149 volume base register (canal A) */
#define YM_PERL(N)	(YM_BASEPERL+(N)*2) /* Canal #N LSB period */
#define YM_PERH(N)	(YM_BASEPERH+(N)*2) /* Canal #N MSB periodr */
#define YM_VOL(N)	(YM_BASEVOL+(N))    /* Canal #N volume */
#define YM_NOISE     	6  /* Noise period */
#define YM_MIXER     	7  /* Mixer control */
#define YM_ENVL      	11 /* Volume envelop LSB period */
#define YM_ENVH      	12 /* Volume envelop MSB period */
#define YM_ENVTYPE   	13 /* Volume envelop wave form */
#define YM_ENVSHAPE  	13 /* Volume envelop wave form */

typedef int32 ymsample;


struct dc_adjuster {
	int m_buffer[DC_ADJUST_BUFFERLEN];
	int m_pos;
	int m_sum;
};


struct ym2149 {
	struct dc_adjuster *dc;

	uint32	frameCycle;
	uint32	cyclePerSample;

	int	replayFrequency;
	uint32	internalClock;
	int	regs[14];

	uint32	cycleSample;
	uint32	stepA,stepB,stepC;
	uint32	posA,posB,posC;
	int	volA,volB,volC,volE;
	uint32	mixerTA,mixerTB,mixerTC;
	uint32	mixerNA,mixerNB,mixerNC;
	int	*pVolA,*pVolB,*pVolC;

	uint32	noiseStep;
	uint32	noisePos;
	uint32	rndRack;
	uint32	currentNoise;
	uint32	bWrite13;

	uint32	envStep;
	uint32	envPos;
	int	envPhase;
	int	envShape;
	uint8	envData[16][2][16*2];
	int	globalVolume;
};

struct ym2149 *ym2149_new(int, int, int);
void ym2149_reset(struct ym2149 *);
void ym2149_update(struct ym2149 *, ymsample *, int, int, int, int);
void ym2149_write_register(struct ym2149 *, int, int);
int ym2149_read_register(struct ym2149 *, int reg);
void ym2149_destroy(struct ym2149 *);

#endif
