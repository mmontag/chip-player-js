/* Extended Module Player OMX depacker
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: oxm.c,v 1.1 2007-09-30 16:28:16 cmatsuoka Exp $
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


static void move_data(FILE *out, FILE *in, int len)
{
	uint8 buf[1024];
	int l;

	do {
		l = fread(buf, 1, len > 1024 ? 1024 : len, in);
		fwrite(buf, 1, l, out);
		len -= l;
	} while (l > 0 && len > 0);
}


int test_oxm(FILE *f)
{
	int i;
	int hlen, npat, len, plen;
	int nins;
	uint8 buf[20];

	fseek(f, 0, SEEK_SET);
	fread(buf, 16, 1, f);
	if (memcmp(buf, "Extended Module:", 16))
		return -1;
	
	fseek(f, 60, SEEK_SET);
	hlen = read32l(f);
	fseek(f, 6, SEEK_CUR);
	npat = read16l(f);
	nins = read16l(f);
	
	fseek(f, 60 + hlen, SEEK_SET);

	for (i = 0; i < npat; i++) {
		len = read32l(f);
		fseek(f, 3, SEEK_CUR);
		plen = read16l(f);
		fseek(f, len - 9 + plen, SEEK_CUR);
	}

	fseek(f, 26, SEEK_CUR);
	if (read8(f) != 0x3f)
		return -1;

	return 0;
}

int decrunch_oxm(FILE *f, FILE *fo)
{
	int i, pos;
	int hlen, npat, len, plen;
	int nins;

	fseek(f, 60, SEEK_SET);
	hlen = read32l(f);
	fseek(f, 6, SEEK_CUR);
	npat = read16l(f);
	nins = read16l(f);
	
	fseek(f, 60 + hlen, SEEK_SET);

	for (i = 0; i < npat; i++) {
		len = read32l(f);
		fseek(f, 3, SEEK_CUR);
		plen = read16l(f);
		fseek(f, len - 9 + plen, SEEK_CUR);
	}

	pos = ftell(f);
	fseek(f, 0, SEEK_SET);
	move_data(fo, f, pos);

	return -1;
}
