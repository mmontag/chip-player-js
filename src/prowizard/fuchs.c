/*
 * FuchsTracker.c   Copyright (C) 1999 Sylvain "Asle" Chipaux
 *                  Modified by Claudio Matsuoka
 *
 * Depacks Fuchs Tracker modules
 *
 * $Id: fuchs.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_fuchs (uint8 *, int);
static int depack_fuchs (FILE *, FILE *);

struct pw_format pw_fchs = {
	"FCHS",
	"Fuchs Tracker",
	0x00,
	test_fuchs,
	NULL,
	depack_fuchs
};


static int depack_fuchs (FILE * in, FILE * out)
{
	uint8 *tmp;
	uint8 c1, c2, c3, c4;
	uint8 PatMax = 0x00;
	long ssize = 0;
	long SampleSizes[16];
	long LoopStart[16];
	long i = 0, j = 0;

	bzero (SampleSizes, 16 * 4);
	bzero (LoopStart, 16 * 4);

	/* write ptk header */
	tmp = (uint8 *) malloc (1080);
	bzero (tmp, 1080);
	fwrite (tmp, 1080, 1, out);
	free (tmp);

	/* read/write title */
	fseek (out, 0, 0);
	for (i = 0; i < 10; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
	}

	/* read all sample data size */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	ssize = ((c1 << 24) +
		(c2 << 16) + (c3 << 8) + c4);
/*  printf ( "Whole Sample Size : %ld\n" , ssize );*/

	/* read/write sample sizes */
	/* have to halve these :( */
	for (i = 0; i < 16; i++) {
		fseek (out, 42 + i * 30, 0);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		SampleSizes[i] = (c3 << 8) + c4;
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
	}

	/* read/write volumes */
	for (i = 0; i < 16; i++) {
		fseek (out, 45 + i * 30, 0);
		fseek (in, 1, 1);
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
	}

	/* read/write loop start */
	/* have to halve these :( */
	for (i = 0; i < 16; i++) {
		fseek (out, 46 + i * 30, 0);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		LoopStart[i] = (c3 << 8) + c4;
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
	}

	/* write replen */
	/* have to halve these :( */
	c3 = 0x01;
	c4 = 0x00;
	for (i = 0; i < 16; i++) {
		fseek (out, 48 + i * 30, 0);
		j = SampleSizes[i] - LoopStart[i];
		if ((j == 0) || (LoopStart[i] == 0)) {
			fwrite (&c4, 1, 1, out);
			fwrite (&c3, 1, 1, out);
			continue;
		}

		j /= 2;
    /*****************************************************/
		/* BEWARE: it's PC only code here !!!                */
		/* 68k machines code : c1 = *(tmp+2);           */
		/* 68k machines code : c2 = *(tmp+3);           */
    /*****************************************************/
		tmp = (uint8 *) & j;
		c1 = *(tmp + 1);
		c2 = *tmp;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}


	/* fill replens up to 31st sample wiz $0001 */
	for (i = 16; i < 31; i++) {
		fseek (out, 48 + i * 30, 0);
		/* rmq: c4 = 0x00 */
		/* rmq: c3 = 0x01 */
		fwrite (&c4, 1, 1, out);
		fwrite (&c3, 1, 1, out);
	}

	/* that's it for the samples ! */
	/* now, the pattern list */

	/* read number of pattern to play */
	fseek (out, 950, 0);
	/* bypass empty byte (saved wiz a WORD ..) */
	fseek (in, 1, 1);
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);

	/* write ntk byte */
	c2 = 0x7f;
	fwrite (&c2, 1, 1, out);

	/* read/write pattern list */
	PatMax = 0;
	for (i = 0; i < 40; i++) {
		fseek (in, 1, 1);
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		if (c1 > PatMax)
			PatMax = c1;
	}

	/* write ptk's ID */
	fseek (out, 0, 2);
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* now, the pattern data */

	/* bypass the "SONG" ID */
	fseek (in, 4, 1);

	/* read pattern data size */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	j = ((c1 << 24) + (c2 << 16) + (c3 << 8) + c4);

	/* read pattern data */
	tmp = (uint8 *) malloc (j);
	fread (tmp, j, 1, in);

	/* convert shits */
	for (i = 0; i < j; i += 4) {
		/* convert fx C arg back to hex value */
		if ((tmp[i + 2] & 0x0f) == 0x0c) {
			c1 = tmp[i + 3];
			if (c1 <= 9) {
				tmp[i + 3] = c1;
				continue;
			}
			if ((c1 >= 16) && (c1 <= 25)) {
				tmp[i + 3] = (c1 - 6);
				continue;
			}
			if ((c1 >= 32) && (c1 <= 41)) {
				tmp[i + 3] = (c1 - 12);
				continue;
			}
			if ((c1 >= 48) && (c1 <= 57)) {
				tmp[i + 3] = (c1 - 18);
				continue;
			}
			if ((c1 >= 64) && (c1 <= 73)) {
				tmp[i + 3] = (c1 - 24);
				continue;
			}
			if ((c1 >= 80) && (c1 <= 89)) {
				tmp[i + 3] = (c1 - 30);
				continue;
			}
			if ((c1 >= 96) && (c1 <= 100)) {
				tmp[i + 3] = (c1 - 36);
				continue;
			}
/*      printf ( "error:vol arg:%x (at:%ld)\n" , c1 , i+200 );*/
		}
	}

	/* write pattern data */
	fwrite (tmp, j, 1, out);
	free (tmp);

	/* read/write sample data */
	fseek (in, 4, 1);	/* bypass "INST" Id */
	for (i = 0; i < 16; i++) {
		if (SampleSizes[i] != 0) {
			tmp = (uint8 *) malloc (SampleSizes[i]);
			fread (tmp, SampleSizes[i], 1, in);
			fwrite (tmp, SampleSizes[i], 1, out);
			free (tmp);
		}
	}

	return 0;
}


static int test_fuchs (uint8 *data, int s)
{
	int start = 0;
	int j, k, m, n, o;

#if 0
	/* test #1 */
	if (i < 192) {
		Test = BAD;
		return;
	}
	start = i - 192;
#endif

	if (data[0]!='S' || data[1]!='O' || data [2]!='N' || data[3]!='G')
		return -1;

	/* all sample size */
	j = ((data[start + 10] << 24) + (data[start + 11] << 16) +
		(data[start + 12] << 8) + data[start + 13]);

	if (j <= 2 || j >= (65535 * 16))
		return -1;

	/* samples descriptions */
	m = 0;
	for (k = 0; k < 16; k++) {
		/* size */
		o = (data[start + k * 2 + 14] << 8) + data[start + k * 2 + 15];
		/* loop start */
		n = (data[start + k * 2 + 78] << 8) + data[start + k * 2 + 79];

		/* volumes */
		if (data[start + 46 + k * 2] > 0x40)
			return -1;

		/* size < loop start ? */
		if (o < n)
			return -1;

		m += o;
	}

	/* m is the size of all samples (in descriptions) */
	/* j is the sample data sizes (header) */
	/* size<2  or  size > header sample size ? */
	if (m <= 2 || m > j)
		return -1;

	/* get highest pattern number in pattern list */
	k = 0;
	for (j = 0; j < 40; j++) {
		n = data[start + j * 2 + 113];
		if (n > 40)
			return -1;
		if (n > k)
			k = n;
	}

	/* m is the size of all samples (in descriptions) */
	/* k is the highest pattern data -1 */

#if 0
	/* input file not long enough ? */
	k += 1;
	k *= 1024;
	PW_REQUEST_DATA (s, k + 200);
#endif

	/* m is the size of all samples (in descriptions) */
	/* k is the pattern data size */

	return 0;
}
