/*
 * TitanicsPlayer.c  Copyright (C) 2007 Sylvain "Asle" Chipaux
 * xmp version Copyright (C) 2009 Claudio Matsuoka
 */


#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_titanics (uint8 *, int);
static int depack_titanics (FILE *, FILE *);

struct pw_format pw_titanics = {
	"TIT",
	"Titanics Player",
	0x00,
	test_titanics,
	depack_titanics
};

/* With the help of Xigh :) .. thx */
static int cmplong(const void *a, const void *b)
{
	long *aa = (long *)a;
	long *bb = (long *)b;

	return *aa == *bb ? 0 : *aa > *bb ? 1 : -1;
}


static int depack_titanics(FILE *in, FILE *out)
{
	uint8 *buf;
	uint8 c1;
	long pat_addr[128];
	long pat_addr_ord[128];
	long pat_addr_final[128];
	long max = 0l;
	uint8 pat;
	uint32 smp_addr[15];
	uint16 smp_size[15];
	int i, j, k;

	memset(pat_addr, 0, 128);
	memset(pat_addr_ord, 0, 128);
	memset(pat_addr_final, 0, 128);

	pw_write_zero(out, 20);			/* write title */

	for (i = 0; i < 15; i++) {		/* only 15 samples */
		smp_addr[i] = read32b(in);
		pw_write_zero(out, 22);		/* write name */
		write16b(out, smp_size[i] = 2 * read16b(in));
		write8(out, read8(in));		/* finetune */
		write8(out, read8(in));		/* volume */
		write16b(out, read16b(in));	/* loop start */
		write16b(out, read16b(in));	/* loop size */
	}

	buf = calloc(1, 2048);

	/* pattern list */
	fread(buf, 2, 128, in);
	for (pat = 0; pat < 128; pat++) {
		if (buf[pat * 2] == 0xff)
			break;
		pat_addr_ord[pat] = pat_addr[pat] = readmem16b(buf + pat * 2);
	}

	write8(out, pat);		/* patterns */
	write8(out, 0);			/* restart byte */

	memset(buf, 0, 2048);

	/* With the help of Xigh :) .. thx */
	qsort(pat_addr_ord, pat, sizeof(long), cmplong);

	for (j = i = 0; i < pat; i++) {
		pat_addr_final[j++] = pat_addr_ord[i];
		while ((pat_addr_ord[i + 1] == pat_addr_ord[i]) && (i < pat))
			i++;
	}

	/* write pattern list */
	for (i = 0; i < pat; i++) {
		for (j = 0; pat_addr[i] != pat_addr_final[j]; j++) ;
		buf[i] = j;
		if (j > max)
			max = j;
	}
	fwrite(buf, 128, 1, out);

	/* pattern data */
	for (i = 0; i <= max; i++) {
		uint8 x, y;

		k = 0;

		fseek(in, SEEK_SET, pat_addr_final[i]);
		//Where = pat_addr_final[i] + PW_Start_Address;

		memset(buf, 0, 2048);
		do {
			x = read8(in);
			y = read8(in);

			/* k is row nbr */

			c1 = ((y >> 6) & 0x03) * 4;	/* voice */

			/* no note ... */
			if ((y & 0x3f) <= 36) {
				buf[(k * 16) + c1] = ptk_table[y & 0x3f][0];
				buf[(k * 16) + c1 + 1] = ptk_table[y & 0x3F][1];
			}
			buf[(k * 16) + c1 + 2] = read8(in);
			buf[(k * 16) + c1 + 3] = read8(in);

			if ((x & 0x7f) != 0)
				k += x & 0x7f;
			if (k > 1024) {
				/*printf ("pat %ld too big\n",i); */
				break;
			}
		} while ((x & 0x80) != 0x80);	/* pattern break it seems */

		fwrite(&buf[0], 1024, 1, out);
	}

	/* Where is now on the first smp */

	free(buf);

	/* sample data */
#if 0
	for (i = 0; i < 15; i++) {
		if (smp_addr[i] != 0)
			fwrite(&in_data[PW_Start_Address + smp_addr[i]],
			       smp_size[i], 1, out);
	}
#endif

	return 0;
}


static int test_titanics(uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

#if 0
	if (i < 7)
		return -1;

	start = i - 7;
#endif

	/* test samples */
	n = ssize = 0;
	for (k = 0; k < 15; k++) {
		if (data[start + 7 + k * 12] > 0x40)
			return -1;
			
		if (data[start + 6 + k * 12] != 0x00)
			return -1;

		o = readmem32b(data + start + k * 12);
		if (/*o > in_size ||*/ (o < 180 && o != 0))
			return -1;

		j = readmem16b(data + start + k * 12 + 4);	/* size */
		l = readmem16b(data + start + k * 12 + 8);	/* loop start */
		m = readmem16b(data + start + k * 12 + 10);	/* loop size */

		if (l > j || m > (j + 1) || j > 32768)
			return -1;

		if (m == 0)
			return -1;

		if (j == 0 && (l != 0 || m != 1))
			return -1;

		ssize += j;
	}

	if (ssize < 2)
		return -1;

	/* test pattern addresses */
	o = -1;
	for (l = k = 0; k < 256; k += 2) {
		if ((data[start + k + 180] == 0xff)
				&& (data[start + k + 181] == 0xff)) {
			o = 0;
			break;
		}

		j = ((data[start + k + 180] * 256) + data[start + k + 181]);
		if (j < 180)
			return -1;

		if (j > l)
			l = j;
	}

	if (o == -1)
		return -1;

	/* l is the max addr of the pattern addrs */
	return 0;
}
