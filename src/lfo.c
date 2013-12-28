/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdlib.h>
#include "lfo.h"

#define WAVEFORM_SIZE 64

static const short sine_wave[WAVEFORM_SIZE] = {
	   0,  24,  49,  74,  97, 120, 141, 161, 180, 197, 212, 224,
	 235, 244, 250, 253, 255, 253, 250, 244, 235, 224, 212, 197,
	 180, 161, 141, 120,  97,  74,  49,  24,   0, -24, -49, -74,
	 -97,-120,-141,-161,-180,-197,-212,-224,-235,-244,-250,-253,
	-255,-253,-250,-244,-235,-224,-212,-197,-180,-161,-141,-120,
	 -97, -74, -49, -24
};

/* LFO */

int get_lfo(struct lfo *lfo)
{
	if (lfo->rate == 0)
		return 0;

	switch (lfo->type) {
	case 0:	/* sine */
		return sine_wave[lfo->phase] * lfo->depth;
	case 1:	/* ramp down */
		return 255 - (lfo->phase << 3);
	case 2:	/* square wave */
		return lfo->phase < WAVEFORM_SIZE / 2 ? 255 * lfo->depth : 0;
	case 3:	/* random */
		return ((rand() & 0x1ff) - 256) * lfo->depth;
	case 4:	/* ramp up */
		return -255 + (lfo->phase << 3);
	default:
		return 0;
	}
	
}

void update_lfo(struct lfo *lfo)
{
	lfo->phase += lfo->rate;
	lfo->phase %= WAVEFORM_SIZE;
}

void set_lfo_phase(struct lfo *lfo, int phase)
{
	lfo->phase = phase;
}

void set_lfo_depth(struct lfo *lfo, int depth)
{
	lfo->depth = depth;
}

void set_lfo_rate(struct lfo *lfo, int rate)
{
	lfo->rate = rate;
}

void set_lfo_waveform(struct lfo *lfo, int type)
{
	lfo->type = type;
}
