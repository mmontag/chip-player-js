
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define __XMP_LOADERS_COMMON
#include "load.h"

#include "archimedes.h"

static int8 table[128] = {
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

void arc2linear(int8 *buf, int len)
{
	int i;
	uint8 x;

	for (i = 0; i < len; i++) {
		x = buf[i];
		buf[i] = table[x >> 1];
		if (x & 0x01)
			buf[i] *= -1;
	}
}
