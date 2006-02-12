/*
 * NoisePacker_v1.c   Copyright (C) 1997 Asle / ReDoX
 *                    Modified by Claudio Matsuoka
 *
 * Converts NoisePacked MODs back to ptk
 *
 * $Id: np1.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_np1 (uint8 *, int);
static int depack_np1 (FILE *, FILE *);

struct pw_format pw_np1 = {
	"np1",
	"NoisePacker v1",
	0x00,
	test_np1,
	NULL,
	depack_np1
};


static int depack_np1 (FILE *in, FILE *out)
{
	uint8 *tmp;
	uint8 c1, c2, c3, c4;
	uint8 npos;
	uint8 nins;
	uint8 ptable[128];
	uint8 pat_max = 0x00;
	int Max_Add = 0;
	int ssize = 0;
	int tsize;
	int taddr[128][4];
	int Unknown1;
	int i = 0, j = 0, k;
	int tdata;

	bzero (ptable, 128);
	bzero (taddr, 128 * 4 * 4);

	/* read number of sample */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	nins = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0f);
	/*printf ( "Number of sample : %d (%x)\n" , nins , nins ); */

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
	Unknown1 = (c1 << 8) + c2;

	/* read track data size */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	tsize = (c1 << 8) + c2;
	/*printf ( "tsize : %ld\n" , tsize ); */

	/* read sample descriptions */
	tmp = (uint8 *) malloc (22);
	bzero (tmp, 22);
	for (i = 0; i < nins; i++) {
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
		c4 /= 2;
		if ((c3 / 2) * 2 != c3) {
			if (c4 < 0x80)
				c4 += 0x80;
			else {
				c4 -= 0x80;
				c3 += 0x01;
			}
		}
		c3 /= 2;
		fwrite (&c3, 1, 1, out);
		fwrite (&c4, 1, 1, out);
		/* write loop size */
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

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
	pat_max = 0x00;
	for (i = 0; i < npos; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		ptable[i] = ((c1 << 8) + c2) / 8;
		if (ptable[i] > pat_max)
			pat_max = ptable[i];
	}
	pat_max += 1;
	/*printf ( "Number of pattern : %d\n" , pat_max ); */

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
	for (i = 0; i < pat_max; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		taddr[i][0] = (c1 << 8) + c2;
		if (taddr[i][0] > Max_Add)
			Max_Add = taddr[i][0];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		taddr[i][1] = (c1 << 8) + c2;
		if (taddr[i][1] > Max_Add)
			Max_Add = taddr[i][1];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		taddr[i][2] = (c1 << 8) + c2;
		if (taddr[i][2] > Max_Add)
			Max_Add = taddr[i][2];
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		taddr[i][3] = (c1 << 8) + c2;
		if (taddr[i][3] > Max_Add)
			Max_Add = taddr[i][3];
	}
	tdata = ftell (in);

	/* the track data now ... */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < pat_max; i++) {
		bzero (tmp, 1024);
		for (j = 0; j < 4; j++) {
			fseek (in,
				tdata +
				taddr[i][3 - j], 0);
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
	fseek (in, Max_Add + 192 + tdata, 0);
	tmp = (uint8 *) malloc (ssize);
	bzero (tmp, ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}

static int test_np1 (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	/* size of the pattern table */
	j = (data[start + 2] << 8) + data[start + 3];
	if (((j / 2) * 2) != j || j == 0)
		return -1;

	/* test nbr of samples */
	if ((data[start + 1] & 0x0f) != 0x0C)
		return -1;

	l = ((data[start] << 4) & 0xf0) | ((data[start + 1] >> 4) & 0x0f);
	if (l > 0x1F || l == 0)
		return -1;
	/* l is the number of samples */

	PW_REQUEST_DATA (s, start + 15 + l * 16);

	/* test volumes */
	for (k = 0; k < l; k++) {
		if (data[start + 15 + k * 16] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (k = 0; k < l; k++) {
		o = (data[start + k*16 + 12] << 8) + data[start + k*16 + 13];
		m = (data[start + k*16 + 20] << 8) + data[start + k*16 + 21];
		n = (data[start + k*16 + 22] << 8) + data[start + k*16 + 23];
		o *= 2;
		m *= 2;

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

	/* small shit to gain some vars */
	l *= 16;
	l += 8;
	l += 4;
	/* l is the size of the header til the end of sample descriptions */

	/* test pattern table */
	n = 0;
	for (k = 0; k < j; k += 2) {
		m = ((data[start + l + k] << 8) + data[start + l + k + 1]);
		if (((m / 8) * 8) != m)
			return -1;
		if (m > n)
			n = m;
	}

	l += j;
	l += n;
	l += 8;		/* paske on a que l'address du dernier pattern .. */
	/* l is now the size of the header 'til the end of the track list */
	/* j is now available for use :) */

	/* test track data size */
	k = (data[start + 6] << 8) + data[start + 7];
	if (k < 192 || ((k / 192) * 192) != k)
		return -1;

	PW_REQUEST_DATA (s, start + l + k);

	/* test notes */
	for (m = 0; m < k; m += 3) {
		if (data[start + l + m] > 0x49)
			return -1;
	}

	/* ssize is the size of the sample data */
	/* l is the size of the header 'til the track datas */
	/* k is the size of the track datas */

	return 0;
}

