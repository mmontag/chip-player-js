/* Extended Module Player UMX depacker
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: umx.c,v 1.2 2007-09-23 01:53:24 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmpi.h"

#define TEST_SIZE 1500

#define MAGIC4(a,b,c,d) \
    (((uint32)(a)<<24)|((uint32)(b)<<16)|((uint32)(c)<<8)|(d))

#define MAGIC_IMPM	MAGIC4('I','M','P','M')
#define MAGIC_SCRM	MAGIC4('S','C','R','M')
#define MAGIC_M_K_	MAGIC4('M','.','K','.')


int decrunch_umx(FILE *f, FILE *fo)
{
	int i;
	uint8 *buf, *b;
	int len, offset = -1;

	if (fo == NULL)
		return -1;

	if ((b = buf = malloc(0x10000)) == NULL)
		return -1;

	fread(buf, 1, TEST_SIZE, f);
	for (i = 0; i < TEST_SIZE; i++, b++) {
		uint32 id;

		id = readmem32b(b);

		if (!memcmp(b, "Extended Module:", 16)) {
			offset = i;
			break;
		}
		if (id == MAGIC_IMPM) { 
			offset = i;
			break;
		}
		if (i > 44 && id == MAGIC_SCRM) { 
			offset = i - 44;
			break;
		}
		if (i > 1080 && id == MAGIC_M_K_) { 
			offset = i - 1080;
			break;
		}
	}
	
	if (offset < 0) {
		free(buf);
		return -1;
	}

	fseek(f, offset, SEEK_SET);

	do {
		len = fread(buf, 1, 0x10000, f);
		fwrite(buf, 1, len, fo);
	} while (len == 0x10000);

	free(buf);

	return 0;
}

