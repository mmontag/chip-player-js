/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "common.h"
#include "envelope.h"

/* Envelope */

int get_envelope(struct xmp_envelope *env, int x, int def, int *end)
{
	int x1, x2, y1, y2;
	int16 *data = env->data;
	int index;

	*end = 0;

	if (~env->flg & XMP_ENVELOPE_ON || env->npt <= 0)
		return def;

	index = (env->npt - 1) * 2;

	x1 = data[index];		/* last node */
	if (x >= x1 || index == 0) { 
		*end = 1;
		return data[index + 1];
	}

	do {
		index -= 2;
		x1 = data[index];
	} while (index > 0 && x1 > x);

	/* interpolate */
	y1 = data[index + 1];
	x2 = data[index + 2];

	if (env->flg & XMP_ENVELOPE_LOOP && index == (env->lpe << 1)) {
		index = (env->lps - 1) * 2;
	}

	y2 = data[index + 3];

	return ((y2 - y1) * (x - x1) / (x2 - x1)) + y1;
}


int update_envelope(struct xmp_envelope *env, int x, int release)
{
	int16 *data = env->data;
	int has_loop, has_sus;
	int lpe, lps, sus, sue;

	if (x < 0xffff)	{	/* increment tick */
		x++;
	}

	if (~env->flg & XMP_ENVELOPE_ON || env->npt <= 0) {
		return x;
	}

	has_loop = env->flg & XMP_ENVELOPE_LOOP;
	has_sus = env->flg & XMP_ENVELOPE_SUS;

	lps = env->lps << 1;
	lpe = env->lpe << 1;
	sus = env->sus << 1;
	sue = env->sue << 1;

	if (env->flg & XMP_ENVELOPE_SLOOP) {
		if (!release && has_sus) {
			if (x > data[sue])
				x = data[sus];
		} else if (has_loop) {
			if (x > data[lpe])
				x = data[lps];
		}
	} else {
		if (!release && has_sus && x > data[sus]) {
			/* stay in the sustain point */
			x = data[sus];
		}

		if (has_loop && x > data[lpe]) {
	    		if (!(release && has_sus && sus == lpe))
				x = data[lps];
		}
	}

	return x;
}


/* Returns: 0 if do nothing, <0 to reset channel, >0 if has fade */
int check_envelope_fade(struct xmp_envelope *env, int x)
{
	int16 *data = env->data;
	int index;

	if (~env->flg & XMP_ENVELOPE_ON)
		return 0;

	index = (env->npt - 1) * 2;		/* last node */
	if (x > data[index]) {
		if (data[index + 1] == 0)
			return -1;
		else
			return 1;
	}

	return 0;
}

