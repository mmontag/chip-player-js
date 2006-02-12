/*
 * TDD.c   Copyright (C) 1999 Asle / ReDoX
 *         Modified by Claudio Matsuoka
 *
 * Converts TDD packed MODs back to PTK MODs
 *
 * $Id: tdd.c,v 1.1 2006-02-12 22:04:43 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_tdd (uint8 *, int);
static int depack_tdd (FILE *, FILE *);

struct pw_format pw_tdd = {
	"tdd",
	"The Dark Demon",
	0x00,
	test_tdd,
	NULL,
	depack_tdd
};


static int depack_tdd (FILE *in, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 *tmp;
	uint8 pat[1024];
	uint8 pmax = 0x00;
	int i = 0, j = 0, k = 0;
	int ssize = 0;
	int saddr[31];
	int ssizes[31];

	bzero (saddr, 31 * 4);
	bzero (ssizes, 31 * 4);

	/* write ptk header */
	tmp = (uint8 *) malloc (1080);
	bzero (tmp, 1080);
	fwrite (tmp, 1080, 1, out);
	free (tmp);

	/* read/write pattern list + size and ntk byte */
	tmp = (uint8 *) malloc (130);
	bzero (tmp, 130);
	fseek (out, 950, 0);
	fread (tmp, 130, 1, in);
	fwrite (tmp, 130, 1, out);
	pmax = 0x00;
	for (i = 0; i < 128; i++)
		if (tmp[i + 2] > pmax)
			pmax = tmp[i + 2];
	free (tmp);

	/* sample descriptions */
	for (i = 0; i < 31; i++) {
		fseek (out, 42 + (i * 30), 0);
		/* sample address */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		saddr[i] = ((c1 << 24) +
			(c2 << 16) + (c3 << 8) + c4);

		/* read/write size */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		ssize += (((c1 << 8) + c2) * 2);
		ssizes[i] = (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		/* read/write finetune */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);

		/* read/write volume */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);

		/* read loop start address */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		j = ((c1 << 24) +
			(c2 << 16) + (c3 << 8) + c4);
		j -= saddr[i];
		j /= 2;
    /*****************************************************/
		/* BEWARE: it's PC only code here !!!                */
		/* 68k machines code : c1 = *(tmp+2);           */
		/* 68k machines code : c2 = *(tmp+3);           */
    /*****************************************************/
		tmp = (uint8 *) & j;
		c1 = *(tmp + 1);
		c2 = *tmp;

		/* write loop start */
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		/* read/write replen */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}

	/* bypass Samples datas */
	fseek (in, ssize, 1);	/* SEEK_CUR */

	/* write ptk's ID string */
	fseek (out, 0, 2);
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* read/write pattern data */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i <= pmax; i++) {
		bzero (tmp, 1024);
		bzero (pat, 1024);
		fread (tmp, 1024, 1, in);
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				/* fx arg */
				pat[j * 16 + k * 4 + 3] =
					tmp[j * 16 + k * 4 + 3];

				/* fx */
				pat[j * 16 + k * 4 + 2] =
					tmp[j * 16 + k * 4 + 2] & 0x0f;

				/* smp */
				pat[j * 16 + k * 4] =
					tmp[j * 16 + k * 4] & 0xf0;
				pat[j * 16 + k * 4 + 2] |=
					(tmp[j * 16 + k * 4] << 4) & 0xf0;

				/* note */
				pat[j * 16 + k * 4] |= ptk_table[tmp[j *
					16 + k * 4 + 1] / 2][0];
				pat[j * 16 + k * 4 + 1] = ptk_table[tmp[j *
					16 + k * 4 + 1] / 2][1];
			}
		}
		fwrite (pat, 1024, 1, out);
	}
	free (tmp);

	/* Sample data */
	for (i = 0; i < 31; i++) {
		if (ssizes[i] == 0l) {
			continue;
		}
		fseek (in, saddr[i], 0);
		tmp = (uint8 *) malloc (ssizes[i]);
		fread (tmp, ssizes[i], 1, in);
		fwrite (tmp, ssizes[i], 1, out);
		free (tmp);
	}

	return 0;
}


static int test_tdd (uint8 *data, int s)
{
	int j, k, l, m, n;
	int start = 0, ssize;

	PW_REQUEST_DATA (s, 564);

	/* test #2 (volumes,sample addresses and whole sample size) */
	ssize = 0;
	for (j = 0; j < 31; j++) {
		/* sample address */
		k = (data[start + j * 14 + 130] << 24) +
			(data[start + j * 14 + 131] << 16) +
			(data[start + j * 14 + 132] << 8) +
			data[start + j * 14 + 133];

		/* sample size */
		l = (((data[start + j * 14 + 134] << 8) +
			data[start + j * 14 + 135]) * 2);

		/* loop start address */
		m = (data[start + j * 14 + 138] << 24) +
			(data[start + j * 14 + 139] << 16) +
			(data[start + j * 14 + 140] << 8) +
			data[start + j * 14 + 141];

		/* loop size (replen) */
		n = (((data[start + j * 14 + 142] << 8) +
			data[start + j * 14 + 143]) * 2);

		/* volume > 40h ? */
		if (data[start + j * 14 + 137] > 0x40)
			return -1;

		/* loop start addy < sampl addy ? */
		if (m < k)
			return -1;

		/* addy < 564 ? */
		if (k < 564 || m < 564)
			return -1;

		/* loop start > size ? */
		if ((m - k) > l)
			return -1;

		/* loop start+replen > size ? */
		if ((m - k + n) > (l + 2))
			return -1;

		ssize += l;
	}

	if (ssize <= 2 || ssize > (31 * 65535))
		return -1;

#if 0
	/* test #3 (addresses of pattern in file ... ptk_tableible ?) */
	/* ssize is the whole sample size :) */
	if ((ssize + 564) > in_size) {
/*printf ( "#3 (start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
#endif

	/* test size of pattern list */
	if (data[start] > 0x7f || data[start] == 0x00)
		return -1;

	/* test pattern list */
	k = 0;
	for (j = 0; j < 128; j++) {
		if (data[start + j + 2] > 0x7f)
			return -1;
		if (data[start + j + 2] > k)
			k = data[start + j + 2];
	}
	k += 1;
	k *= 1024;

	/* test end of pattern list */
	for (j = data[start] + 2; j < 128; j++) {
		if (data[start + j + 2] != 0)
			return -1;
	}

#if 0
	/* test if not out of file range */
	if ((start + ssize + 564 + k) > in_size)
		return -1;
#endif

	/* ssize is the whole sample data size */
	/* k is the whole pattern data size */
	/* test pattern data now ... */
	l = start + 564 + ssize;
	/* l points on pattern data */

	for (j = 0; j < k; j += 4) {
		/* sample number > 31 ? */
		if (data[l + j] > 0x1f)
			return -1;

		/* note > 0x48 (36*2) */
		if (data[l + j + 1] > 0x48 || (data[l + j + 1] & 0x01) == 0x01)
			return -1;

		/* fx=C and fxtArg > 64 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0c && data[l + j + 3] > 0x40)
			return -1;

		/* fx=D and fxtArg > 64 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0d && data[l + j + 3] > 0x40)
			return -1;

		/* fx=B and fxtArg > 127 ? */
		if ((data[l + j + 2] & 0x0f) == 0x0b)
			return -1;
	}

	return -1;
}
