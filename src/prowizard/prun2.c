/*
 * ProRunner2.c   Copyright (C) 1996-1999 Asle / ReDoX
 *                Modified by Claudio Matsuoka
 *
 * Converts ProRunner v2 packed MODs back to Protracker
 ********************************************************
 * 12 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 *
 * $Id: prun2.c,v 1.1 2006-02-12 22:04:43 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_pru2 (uint8 *, int);
static int depack_pru2 (uint8 *, FILE *);

struct pw_format pw_pru2 = {
	"PRU2",
	"Prorunner 2.0",
	0x00,
	test_pru2,
	depack_pru2,
	NULL
};


static int depack_pru2 (uint8 *data, FILE *out)
{
	uint8 Header[2048];
	uint8 c1, c2, c3, c4;
	uint8 npat = 0x00;
	uint8 ptable[128];
	uint8 Max = 0x00;
	uint8 v[4][4];
	int ssize = 0;
	int i = 0, j = 0;
	int start = 0;
	int w = start;	/* main pointer to prevent fread() */

	bzero (Header, 2048);
	bzero (ptable, 128);

	for (i = 0; i < 20; i++)	/* title */
		fwrite (&c1, 1, 1, out);

	w += 4;
	c1 = data[w++];
	c2 = data[w++];
	c3 = data[w++];
	c4 = data[w++];
	i = c3;
	i *= 256;
	i += c4;

	for (i = 0; i < 31; i++) {
		c1 = 0x00;
		for (j = 0; j < 22; j++)	/*sample name */
			fwrite (&c1, 1, 1, out);

		c1 = data[w++];	/* size */
		c2 = data[w++];
		ssize += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		c1 = data[w++];	/* finetune */
		fwrite (&c1, 1, 1, out);
		c1 = data[w++];	/* volume */
		fwrite (&c1, 1, 1, out);
		c1 = data[w++];	/* loop start */
		c2 = data[w++];
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		c1 = data[w++];	/* loop size */
		c2 = data[w++];
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	npat = data[w++];
	fwrite (&npat, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , npat ); */

	/* noisetracker byte */
	c1 = data[w++];
	fwrite (&c1, 1, 1, out);

	for (i = 0; i < 128; i++) {
		c1 = data[w++];
		fwrite (&c1, 1, 1, out);
		Max = (c1 > Max) ? c1 : Max;
	}
	/*printf ( "Number of pattern : %d\n" , Max ); */

	c1 = 'M';
	c2 = '.';
	c3 = 'K';

	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data stuff */
	w = start + 770;
	for (i = 0; i <= Max; i++) {
		for (j = 0; j < 256; j++) {
			c1 = c2 = c3 = c4 = 0x00;
			Header[0] = data[w++];
			if (Header[0] == 0x80) {
				fwrite (&c1, 1, 1, out);
				fwrite (&c1, 1, 1, out);
				fwrite (&c1, 1, 1, out);
				fwrite (&c1, 1, 1, out);
			} else if (Header[0] == 0xC0) {
				fwrite (v[0], 4, 1, out);
				c1 = v[0][0];
				c2 = v[0][1];
				c3 = v[0][2];
				c4 = v[0][3];
			} else if ((Header[0] != 0xC0) && (Header[0] != 0xC0)) {
				Header[1] = data[w++];
				Header[2] = data[w++];

				c1 = (Header[1] & 0x80) >> 3;
				c1 |= ptk_table[(Header[0] >> 1)][0];
				c2 = ptk_table[(Header[0] >> 1)][1];
				c3 = (Header[1] & 0x70) << 1;
				c3 |= (Header[0] & 0x01) << 4;
				c3 |= (Header[1] & 0x0f);
				c4 = Header[2];

				fwrite (&c1, 1, 1, out);
				fwrite (&c2, 1, 1, out);
				fwrite (&c3, 1, 1, out);
				fwrite (&c4, 1, 1, out);
			}

			/* rol previous values */
			v[0][0] = v[1][0];
			v[0][1] = v[1][1];
			v[0][2] = v[1][2];
			v[0][3] = v[1][3];

			v[1][0] = v[2][0];
			v[1][1] = v[2][1];
			v[1][2] = v[2][2];
			v[1][3] = v[2][3];

			v[2][0] = v[3][0];
			v[2][1] = v[3][1];
			v[2][2] = v[3][2];
			v[2][3] = v[3][3];

			v[3][0] = c1;
			v[3][1] = c2;
			v[3][2] = c3;
			v[3][3] = c4;
		}
	}

	/* sample data */
	fwrite (&data[w], ssize, 1, out);

	return 0;
}


static int test_pru2 (uint8 *data, int s)
{
	int k;
	int start = 0;

	PW_REQUEST_DATA (s, 12 + 31 * 8);

	if (data[0]!='S' || data[1]!='N' || data[2]!='T' || data[3]!='!')
		return -1;

#if 0
	/* check sample address */
	j = (data[i + 4] << 24) + (data[i + 5] << 16) + (data[i + 6] << 8) +
		data[i + 7];

	PW_REQUEST_DATA (s, j);
#endif

	/* test volumes */
	for (k = 0; k < 31; k++) {
		if (data[start + 11 + k * 8] > 0x40)
			return -1;
	}

	/* test finetunes */
	for (k = 0; k < 31; k++) {
		if (data[start + 10 + k * 8] > 0x0F)
			return -1;
	}

	return 0;
}
