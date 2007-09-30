/* Extended Module Player OMX depacker
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: oxm.c,v 1.2 2007-09-30 19:04:12 cmatsuoka Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

static char *oggdec(FILE *f, int len, int *newlen)
{
	char line[256];
	char buf[1024];
	char *temp, *pcm;
	FILE *p;
	int fd, l;
	struct stat st;

	temp = strdup("/tmp/xmp_oxm_XXXXXX");

	if ((fd = mkstemp(temp)) < 0)
		goto err1;

	snprintf(line, 256, "oggdec -Q -b16 -e0 -R -s1 -o%s -", temp);
	p = popen(line, "w");

	do {
		l = len > 1024 ? 1024 : len;
		fread(buf, 1, l, f);
		fwrite(buf, 1, l, p);
		len -= l;
	} while (l > 0 && len > 0);
	fclose(p);

	p = fopen(temp, "r");

	if (fstat(fileno(p), &st) < 0)
		goto err2;

	if ((pcm = malloc(st.st_size)) == NULL)
		goto err2;

	fread(pcm, 1, st.st_size, p);

	*newlen = st.st_size / 2;

	fclose(p);
	unlink(temp);
	free(temp);

	return pcm;

err2:
	fclose(p);
	unlink(temp);
err1:
	free(temp);
	return NULL;
}

int decrunch_oxm(FILE *f, FILE *fo)
{
	int i, j, pos;
	int hlen, npat, len, plen;
	int nins, nsmp, ilen, slen[256];
	uint8 buf[1024];
	char *pcm;
	int newlen;

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
	move_data(fo, f, pos);			/* module header + patterns */

	for (i = 0; i < nins; i++) {
		ilen = read32l(f);
		//printf("%lx: inst %d: len %x\n", ftell(f), i, ilen);
		if (ilen > 1024)
			return -1;
		fseek(f, -4, SEEK_CUR);
		fread(buf, ilen, 1, f);		/* instrument header */
		buf[26] = 0;
		fwrite(buf, ilen, 1, fo);
		nsmp = readmem16l(buf + 27);

		if (nsmp == 0)
			continue;

		for (j = 0; j < nsmp; j++) {
			slen[j] = read32l(f);
			//printf("%lx: %x\n", ftell(f), slen[j]);
			fseek(f, -4, SEEK_CUR);
			fread(buf, 40, 1, f);
			fwrite(buf, 40, 1, fo);
		}

		for (j = 0; j < nsmp; j++) {
			pcm = oggdec(f, slen[j], &newlen);
			if (pcm == NULL)
				continue;
			fwrite(pcm, 1, slen[j], fo);
		}
	}

	return 0;
}
