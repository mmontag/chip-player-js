/* Extended Module Player
 * Copyright (C) 1996-2016 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "depacker.h"
#include "inflate.h"

static int test_muse(unsigned char *b)
{
	if (memcmp(b, "MUSE", 4) == 0) {
		uint32 r = readmem32b(b + 4);
		/* MOD2J2B uses 0xdeadbabe */
		if (r == 0xdeadbeaf || r == 0xdeadbabe) {
			return 1;
		}
	}

	return 0;
}

static int decrunch_muse(FILE *f, FILE *fo)                          
{                                                          
	uint32 checksum;
  
	if (fseek(f, 24, SEEK_SET) < 0) {
		return -1;
	}

	return libxmp_inflate(f, fo, &checksum, 0);
}

struct depacker libxmp_depacker_muse = {
	test_muse,
	decrunch_muse
};
