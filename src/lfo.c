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

static const int waveform[4][WAVEFORM_SIZE] = {
	{   0,  24,  49,  74,  97, 120, 141, 161, 180, 197, 212, 224,
	  235, 244, 250, 253, 255, 253, 250, 244, 235, 224, 212, 197,
	  180, 161, 141, 120,  97,  74,  49,  24,   0, -24, -49, -74,
	  -97,-120,-141,-161,-180,-197,-212,-224,-235,-244,-250,-253,
	 -255,-253,-250,-244,-235,-224,-212,-197,-180,-161,-141,-120,
	  -97, -74, -49, -24  },	/* Sine */

	{   0,  -8, -16, -24, -32, -40, -48, -56, -64, -72, -80, -88,
	  -96,-104,-112,-120,-128,-136,-144,-152,-160,-168,-176,-184,
	 -192,-200,-208,-216,-224,-232,-240,-248, 255, 248, 240, 232,
	  224, 216, 208, 200, 192, 184, 176, 168, 160, 152, 144, 136,
	  128, 120, 112, 104,  96,  88,  80,  72,  64,  56,  48,  40,
	   32,  24,  16,   8  },	/* Ramp down */

	{ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	  255, 255, 255, 255, 255, 255, 255, 255,-255,-255,-255,-255,
	 -255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,
	 -255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,-255,
	 -255,-255,-255,-255  },	/* Square */

	{   0,   8,  16,  24,  32,  40,  48,  56,  64,  72,  80,  88,
	   96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184,
	  192, 200, 208, 216, 224, 232, 240, 248,-255,-248,-240,-232,
	 -224,-216,-208,-200,-192,-184,-176,-168,-160,-152,-144,-136,
	 -128,-120,-112,-104, -96, -88, -80, -72, -64, -56, -48, -40,
	  -32, -24, -16,  -8  }		/* Ramp up */
};

/* LFO */

int get_lfo(struct lfo *lfo)
{
	if (lfo->rate == 0)
		return 0;

	switch (lfo->type) {
	case 0:
	case 1:
	case 2:
	case 4:
		return waveform[lfo->type][lfo->phase] * lfo->depth;
	case 3:
		return ((rand() & 0x1ff) - 256) * lfo->depth;
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
