/*
 * NoisePacker_v2.c   Copyright (C) 1997 Asle / ReDoX
 *                    Modified by Claudio Matsuoka
 *
 * Converts NoisePacked MODs back to ptk
 *
 * $Id: np2.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_np2 (uint8 *, int);
static int depack_np2 (FILE *, FILE *);

struct pw_format pw_np2 = {
	"NP2",
	"Noisepacker v2",
	0x00,
	test_np2,
	NULL,
	depack_np2
};

static int depack_np2 (FILE *in, FILE *out)
{
	uint8 *tmp;
	uint8 c1, c2, c3, c4;
	uint8 npos;
	uint8 nsmp;
	uint8 ptable[128];
	uint8 npat = 0x00;
	long Max_Add = 0;
	long ssize = 0;
	long tsize;
	long trk_addr[128][4];
	long x;
	long i = 0, j = 0, k;
	long trk_start;

	bzero (ptable, 128);
	bzero (trk_addr, 128 * 4 * 4);

	/* read number of sample */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	nsmp = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0f);
	/*printf ( "Number of sample : %d (%x)\n" , nsmp , nsmp ); */

	/* write title */
	tmp = (uint8 *) malloc (20);
	bzero (tmp, 20);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/* read size of pattern list */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	npos = c2 / 2;
	/*printf ( "Size of pattern list : %d\n" , npos ); */

	/* read 2 unknown bytes which size seem to be of some use ... */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	x = (c1 << 8) + c2;

	/* read track data size */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	tsize = (c1 << 8) + c2;
	/*printf ( "tsize : %ld\n" , tsize ); */

	/* read sample descriptions */
	tmp = (uint8 *) malloc (22);
	bzero (tmp, 22);
	for (i = 0; i < nsmp; i++) {
		/* bypass 4 unknown bytes */
		fseek (in, 4, 1);
		/* sample name */
		fwrite (tmp, 22, 1, out);
		/* size */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		ssize += (((c1 << 8) + c2) * 2);
		/* finetune */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		/* volume */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		/* bypass 4 unknown bytes */
		fseek (in, 4, 1);
		/* read loop size */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		/* read loop start */
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		/* write loop start */
		fwrite (&c3, 1, 1, out);
		fwrite (&c4, 1, 1, out);
		/* write loop size */
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/* printf ( "Whole sample size : %ld\n" , ssize ); */

	/* fill up to 31 samples */
	free (tmp);
	tmp = (uint8 *) malloc (30);
	bzero (tmp, 30);
	tmp[29] = 0x01;
	while (i != 31) {
		fwrite (tmp, 30, 1, out);
		i += 1;
	}
	free (tmp);

	/* write size of pattern list */
	fwrite (&npos, 1, 1, out);

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* bypass 2 bytes ... seems always the same as in $02 */
	fseek (in, 2, 1);

	/* bypass 2 other bytes which meaning is beside me */
	fseek (in, 2, 1);

	/* read pattern table */
	npat = 0x00;
	for (i = 0; i < npos; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		ptable[i] = ((c1 << 8) + c2) / 8;
		if (ptable[i] > npat)
			npat = ptable[i];
	}
	npat += 1;
	/*printf ( "Number of pattern : %d\n" , npat ); */

	/* write pattern table */
	fwrite (ptable, 128, 1, out);

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* read tracks addresses per pattern */
	for (i = 0; i < npat; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		trk_addr[i][0] = (c1 << 8) + c2;
		if (trk_addr[i][0] > Max_Add)
			Max_Add = trk_addr[i][0];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		trk_addr[i][1] = (c1 << 8) + c2;
		if (trk_addr[i][1] > Max_Add)
			Max_Add = trk_addr[i][1];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		trk_addr[i][2] = (c1 << 8) + c2;
		if (trk_addr[i][2] > Max_Add)
			Max_Add = trk_addr[i][2];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		trk_addr[i][3] = (c1 << 8) + c2;
		if (trk_addr[i][3] > Max_Add)
			Max_Add = trk_addr[i][3];
	}
	trk_start = ftell (in);

	/* the track data now ... */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < npat; i++) {
		bzero (tmp, 1024);
		for (j = 0; j < 4; j++) {
			fseek (in,
				trk_start +
				trk_addr[i][3 - j], 0);
			for (k = 0; k < 64; k++) {
				fread (&c1, 1, 1, in);
				fread (&c2, 1, 1, in);
				fread (&c3, 1, 1, in);
				tmp[k * 16 + j * 4] = (c1 << 4) & 0x10;
				c4 = (c1 & 0xFE) / 2;
				tmp[k * 16 + j * 4] |= ptk_table[c4][0];
				tmp[k * 16 + j * 4 + 1] = ptk_table[c4][1];
				if ((c2 & 0x0f) == 0x08)
					c2 &= 0xf0;
				if ((c2 & 0x0f) == 0x07) {
					c2 = (c2 & 0xf0) + 0x0A;
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x06) {
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x05) {
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x0E) {
					c3 -= 0x01;
				}
				if ((c2 & 0x0f) == 0x0B) {
					c3 += 0x04;
					c3 /= 2;
				}
				tmp[k * 16 + j * 4 + 2] = c2;
				tmp[k * 16 + j * 4 + 3] = c3;
			}
		}
		fwrite (tmp, 1024, 1, out);
	}
	free (tmp);

	/* sample data */
	fseek (in, Max_Add + 192 + trk_start, 0);
	tmp = (uint8 *) malloc (ssize);
	bzero (tmp, ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}


static int test_np2 (uint8 *data, int s)
{
	int j, k, l, o, m, n;
	int start, ssize;

	PW_REQUEST_DATA (s, 1024);

#if 0
	if (i < 15) {
		Test = BAD;
		return;
	}
#endif

	start = 0;

	/* j is the size of the pattern list (*2) */
	j = (data[start + 2] << 8) + data[start + 3];
	if (j & 1 || j == 0)
		return - 1;

	/* test nbr of samples */
	if ((data[start + 1] & 0x0f) != 0x0C)
		return -1;

	/* l is the number of samples */
	l = ((data[start] << 4) & 0xf0) | ((data[start + 1] >> 4) & 0x0f);
	if (l > 0x1F || l == 0)
		return -1;

	/* test volumes */
	for (k = 0; k < l; k++) {
		if (data[start + 15 + k * 16] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (k = 0; k < l; k++) {
		o = 2 * ((data[start + k * 16 + 12] << 8) +
			data[start + k * 16 + 13]);
		m = 2 * ((data[start + k * 16 + 20] << 8) +
			data[start + k * 16 + 21]);
		n = 2 * ((data[start + k * 16 + 22] << 8) +
			data[start + k * 16 + 23]);

		if (o > 0xFFFF || m > 0xFFFF || n > 0xFFFF)
			return -1;

		if ((m + n) > (o + 2))
			return -1;

		if (n != 0 && m == 0)
			return -1;

		ssize += o;
	}

	if (ssize <= 4)
		return -1;

	l *= 16;
	l += 8 + 4;
	/* l is now the size of the header 'til the end of sample descriptions */

	/* test pattern table */
	n = 0;
	for (k = 0; k < j; k += 2) {
		m = ((data[start + l + k] << 8) + data[start + l + k + 1]);
		if (((m / 8) * 8) != m)
			return -1;
		if (m > n)
			n = m;
	}

	l += j + n + 8;

	/* paske on a que l'address du dernier pattern... */
	/* l is now the size of the header 'til the end of the track list */
	/* n is the highest pattern number (*8) */

	/* test track data size */
	k = (data[start + 6] << 8) + data[start + 7];
	if (k < 192 || ((k / 192) * 192) != k)
		return -1;

	PW_REQUEST_DATA (s, k + l + 16);

	/* test notes */
	j = ((data[start] << 4) & 0xf0) | ((data[start + 1] >> 4) & 0x0f);
	for (m = 0; m < k; m += 3) {
		if (data[start + l + m] > 0x49) {
			printf ("Fail 1 on m = %d\n", m);
			return -1;
		}

		if ((((data[start + l + m] << 4) & 0x10) |
			((data[start + l + m + 1] >> 4) & 0x0f)) > j) {
			printf ("Fail 2 on m = %d", m);
			return -1;
		}

		n = (data[start + l + m + 1] & 0x0F);
		if (n == 0 && data[start + l + m + 2] != 0x00) {
			printf ("Fail 3 on m = %d", m);
			return -1;
		}
	}

	/* ssize is the size of the sample data */
	/* l is the size of the header 'til the track datas */
	/* k is the size of the track datas */

	return 0;
}
