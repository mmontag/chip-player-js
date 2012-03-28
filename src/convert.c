/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "convert.h"
#include "virtual.h"


/*
 * From the Audio File Formats (version 2.5)
 * Submitted-by: Guido van Rossum <guido@cwi.nl>
 * Last-modified: 27-Aug-1992
 *
 * The Acorn Archimedes uses a variation on U-LAW with the bit order
 * reversed and the sign bit in bit 0.  Being a 'minority' architecture,
 * Arc owners are quite adept at converting sound/image formats from
 * other machines, and it is unlikely that you'll ever encounter sound in
 * one of the Arc's own formats (there are several).
 */
static const int8 vdic_table[128] = {
	/*   0 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*   8 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  16 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  24 */	  1,   1,   1,   1,   1,   1,   1,   1,
	/*  32 */	  1,   1,   1,   1,   2,   2,   2,   2,
	/*  40 */	  2,   2,   2,   2,   3,   3,   3,   3,
	/*  48 */	  3,   3,   4,   4,   4,   4,   5,   5,
	/*  56 */	  5,   5,   6,   6,   6,   6,   7,   7,
	/*  64 */	  7,   8,   8,   9,   9,  10,  10,  11,
	/*  72 */	 11,  12,  12,  13,  13,  14,  14,  15,
	/*  80 */	 15,  16,  17,  18,  19,  20,  21,  22,
	/*  88 */	 23,  24,  25,  26,  27,  28,  29,  30,
	/*  96 */	 31,  33,  34,  36,  38,  40,  42,  44,
	/* 104 */	 46,  48,  50,  52,  54,  56,  58,  60,
	/* 112 */	 62,  65,  68,  72,  77,  80,  84,  91,
	/* 120 */	 95,  98, 103, 109, 114, 120, 126, 127
};


/* Convert differential to absolute sample data */
void convert_delta(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;
	uint16 abs = 0;

	if (r) {
		for (; l--;) {
			abs = *w + abs;
			*w++ = abs;
		}
	} else {
		for (; l--;) {
			abs = *p + abs;
			*p++ = (uint8) abs;
		}
	}
}

/* Convert signed to unsigned sample data */
void convert_signal(uint8 *p, int l, int r)
{
	uint16 *w = (uint16 *)p;

	if (r) {
		for (; l--; w++)
			*w += 0x8000;
	} else {
		for (; l--; p++)
			*p += (char)0x80;	/* cast needed by MSVC++ */
	}
}

/* Convert little-endian 16 bit samples to big-endian */
void convert_endian(uint8 *p, int l)
{
	uint8 b;
	int i;

	for (i = 0; i < l; i++) {
		b = p[0];
		p[0] = p[1];
		p[1] = b;
		p += 2;
	}
}

/* Downmix stereo samples to mono */
void convert_stereo_to_mono(uint8 *p, int l, int r)
{
	int16 *b = (int16 *)p;
	int i;

	if (r) {
		l /= 2;
		for (i = 0; i < l; i++)
			b[i] = (b[i * 2] + b[i * 2 + 1]) / 2;
	} else {
		for (i = 0; i < l; i++)
			p[i] = (p[i * 2] + p[i * 2 + 1]) / 2;
	}
}

/* Convert 7 bit samples to 8 bit */
void convert_7bit_to_8bit(uint8 *p, int l)
{
	for (; l--; p++) {
		*p <<= 1;
	}
}

/* Convert Archimedes VIDC samples to linear */
void convert_vidc_to_linear(uint8 *p, int l)
{
	int i;
	uint8 x;

	for (i = 0; i < l; i++) {
		x = p[i];
		p[i] = vdic_table[x >> 1];
		if (x & 0x01)
			p[i] *= -1;
	}
}

/* Convert HSC OPL2 instrument data to SBI instrument data */
void convert_hsc_to_sbi(char *a)
{
	char b[11];
	int i;

	for (i = 0; i < 10; i += 2) {
		uint8 x;
		x = a[i];
		a[i] = a[i + 1];
		a[i + 1] = x;
	}

	memcpy(b, a, 11);
	a[8] = b[10];
	a[10] = b[9];
	a[9] = b[8];
}
