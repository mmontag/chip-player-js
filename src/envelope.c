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

int get_envelope(struct xmp_envelope *env, int x, int def)
{
	int x1, x2, y1, y2;
	int16 *data = env->data;
	int index;

	if (~env->flg & XMP_ENVELOPE_ON)
		return def;

	if (env->npt <= 0)
		return 64;

	index = (env->npt - 1) * 2;

	x1 = data[index];		/* last node */
	if (x >= x1 || index == 0) { 
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


int update_envelope(struct xmp_envelope *ei, int x, int release)
{
	int16 *env = ei->data;
	int has_loop, has_sus;

	if (x < 0xffff)	{	/* increment tick */
		x++;
	}

	if (~ei->flg & XMP_ENVELOPE_ON) {
		return x;
	}

	if (ei->npt <= 0) {
		return x;
	}

	if (ei->lps >= ei->npt || ei->lpe >= ei->npt) {
		has_loop = 0;
	} else {
		has_loop = ei->flg & XMP_ENVELOPE_LOOP;
	}

	has_sus = ei->flg & XMP_ENVELOPE_SUS;

	if (ei->flg & XMP_ENVELOPE_SLOOP) {
		if (!release && has_sus) {
			if (x > env[ei->sue << 1])
				x = env[ei->sus << 1];
		} else if (has_loop) {
			if (x > env[ei->lpe << 1])
				x = env[ei->lps << 1];
		}
	} else {
		if (!release && has_sus && x > env[ei->sus << 1]) {
			/* stay in the sustain point */
			x = env[ei->sus << 1];
		}

		if (has_loop && x > env[ei->lpe << 1]) {
	    		if (!(release && has_sus && ei->sus == ei->lpe))
				x = env[ei->lps << 1];
		}
	}

	return x;
}


/* Returns: 0 if do nothing, <0 to reset channel, >0 if has fade */
int check_envelope_fade(struct xmp_envelope *ei, int x)
{
	int16 *env = ei->data;
	int index;

	if (~ei->flg & XMP_ENVELOPE_ON)
		return 0;

	index = (ei->npt - 1) * 2;		/* last node */
	if (x > env[index]) {
		if (env[index + 1] == 0)
			return -1;
		else
			return 1;
	}

	return 0;
}

