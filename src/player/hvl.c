/* Extended Module Player
 * Copyright (C) 1996-2010 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "player.h"

void xmp_hvl_effects(struct xmp_context *ctx, struct xmp_channel *xc)
{
	if (xc->ahx_fon && --xc->ahx_fwait <= 0) {
		uint32 i, FMax;
		int32 d1, d2, d3;

		d1 = xc->ahx_fmin;
		d2 = xc->ahx_fmax;
		d3 = xc->ahx_fpos;

		if (xc->ahx_finit) {
			xc->ahx_finit = 0;
			if (d3 <= d1) {
				xc->ahx_fslide = 1;
				xc->ahx_fsign = 1;
			} else if (d3 >= d2) {
				xc->ahx_fslide = 1;
				xc->ahx_fsign = -1;
			}
		}

		/* NoFilterInit */
		FMax = (xc->ahx_fspeed < 3) ? (5 - xc->ahx_fspeed) : 1;

		for (i = 0; i < FMax; i++) {
			if ((d1 == d3) || (d2 == d3)) {
				if (xc->ahx_fslide)
					xc->ahx_fslide = 0;
				else
					xc->ahx_fsign = -xc->ahx_fsign;
			}
			d3 += xc->ahx_fsign;
		}

		if (d3 < 1)
			d3 = 1;
		if (d3 > 63)
			d3 = 63;
		xc->ahx_fpos = d3;
		xc->ahx_newwave = 1;
		xc->ahx_fwait = xc->ahx_fspeed - 3;

		if (xc->ahx_fwait < 1)
			xc->ahx_fwait = 1;
	}

#if 0
	if (xc->ahx_wave == 3 - 1 || xc->vc_PlantSquare) {
		// CalcSquare
		uint32 i;
		int32 Delta;
		int8 *SquarePtr;
		int32 X;

		SquarePtr =
		    &waves[WO_SQUARES +
			   (xc->ahx_fpos - 0x20) * (0xfc + 0xfc +
							   0x80 * 0x1f + 0x80 +
							   0x280 * 3)];
		X = xc->vc_SquarePos << (5 - xc->vc_WaveLength);

		if (X > 0x20) {
			X = 0x40 - X;
			xc->vc_SquareReverse = 1;
		}
		// OkDownSquare
		if (X > 0)
			SquarePtr += (X - 1) << 7;

		Delta = 32 >> xc->vc_WaveLength;
		ht->ht_WaveformTab[2] = xc->vc_SquareTempBuffer;

		for (i = 0; i < (1 << xc->vc_WaveLength) * 4; i++) {
			xc->vc_SquareTempBuffer[i] = *SquarePtr;
			SquarePtr += Delta;
		}

		xc->ahx_newwave = 1;
		xc->ahx_wave = 3 - 1;
		xc->vc_PlantSquare = 0;
	}

	if (xc->ahx_wave == 4 - 1)
		xc->ahx_newwave = 1;

	if (xc->vc_RingNewWaveform) {
		int8 *rasrc;

		if (xc->vc_RingWaveform > 1)
			xc->vc_RingWaveform = 1;

		rasrc = ht->ht_WaveformTab[xc->vc_RingWaveform];
		rasrc += Offsets[xc->vc_WaveLength];

		xc->vc_RingAudioSource = rasrc;
	}

	if (xc->ahx_newwave) {
		int8 *AudioSource;

		AudioSource = ht->ht_WaveformTab[xc->ahx_wave];

		if (xc->ahx_wave != 3 - 1)
			AudioSource +=
			    (xc->ahx_fpos - 0x20) * (0xfc + 0xfc +
							    0x80 * 0x1f + 0x80 +
							    0x280 * 3);

		if (xc->ahx_wave < 3 - 1) {
			// GetWLWaveformlor2
			AudioSource += Offsets[xc->vc_WaveLength];
		}

		if (xc->ahx_wave == 4 - 1) {
			// AddRandomMoving
			AudioSource +=
			    (xc->vc_WNRandom & (2 * 0x280 - 1)) & ~1;
			// GoOnRandom
			xc->vc_WNRandom += 2239384;
			xc->vc_WNRandom =
			    ((((xc->vc_WNRandom >> 8) | (xc->
							    vc_WNRandom << 24))
			      + 782323) ^ 75) - 6735;
		}

		xc->vc_AudioSource = AudioSource;
	}
	// Ring modulation period calculation
	if (xc->vc_RingAudioSource) {
		xc->vc_RingAudioPeriod = xc->vc_RingBasePeriod;

		if (!(xc->vc_RingFixedPeriod)) {
			if (xc->vc_OverrideTranspose != 1000)	// 1.5
				xc->vc_RingAudioPeriod +=
				    xc->vc_OverrideTranspose +
				    xc->vc_TrackPeriod - 1;
			else
				xc->vc_RingAudioPeriod +=
				    xc->vc_Transpose +
				    xc->vc_TrackPeriod - 1;
		}

		if (xc->vc_RingAudioPeriod > 5 * 12)
			xc->vc_RingAudioPeriod = 5 * 12;

		if (xc->vc_RingAudioPeriod < 0)
			xc->vc_RingAudioPeriod = 0;

		xc->vc_RingAudioPeriod =
		    period_tab[xc->vc_RingAudioPeriod];

		if (!(xc->vc_RingFixedPeriod))
			xc->vc_RingAudioPeriod +=
			    xc->vc_PeriodSlidePeriod;

		xc->vc_RingAudioPeriod +=
		    xc->vc_PeriodPerfSlidePeriod + xc->vc_VibratoPeriod;

		if (xc->vc_RingAudioPeriod > 0x0d60)
			xc->vc_RingAudioPeriod = 0x0d60;

		if (xc->vc_RingAudioPeriod < 0x0071)
			xc->vc_RingAudioPeriod = 0x0071;
	}
#endif

}
