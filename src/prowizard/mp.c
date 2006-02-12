/*
 * Module_Protector.c   Copyright (C) 1997 Asle / ReDoX
 *                      Modified by Claudio Matsuoka
 *
 * Converts MP packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * NOTE: It takes care of both MP packed files with or without ID !
 *
 * $Id: mp.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <stdlib.h>
#include <string.h>
#include "prowiz.h"

static int depack_MP (FILE *, FILE *);
static int test_MP_ID (uint8 *, int);
static int test_MP_noID (uint8 *, int);

struct pw_format pw_mp_id = {
	"MP",
	"Module Protector",
	0x00,
	test_MP_ID,
	NULL,
	depack_MP
};

struct pw_format pw_mp_noid = {
	"MP",
	"Module Protector (no ID)",
	0x00,
	test_MP_noID,
	NULL,
	depack_MP
};


static int depack_MP (FILE *in, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 ptable[128];
	uint8 max = 0x00;
	uint8 *t;
	long i = 0, j = 0;
	long ssize = 0;

	bzero (ptable, 128);

	for (i = 0; i < 20; i++)	/* title */
		fwrite (&c1, 1, 1, out);

	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	if (c1 != 'T' || c2 != 'R' || c3 != 'K' || c4 != '1')
		fseek (in, -4, SEEK_CUR);

	for (i = 0; i < 31; i++) {
		c1 = 0x00;
		for (j = 0; j < 22; j++)	/*sample name */
			fwrite (&c1, 1, 1, out);

		fread (&c1, 1, 1, in);	/* size */
		fread (&c2, 1, 1, in);
		ssize += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		fread (&c1, 1, 1, in);	/* finetune */
		fwrite (&c1, 1, 1, out);
		fread (&c1, 1, 1, in);	/* volume */
		fwrite (&c1, 1, 1, out);
		fread (&c1, 1, 1, in);	/* loop start */
		fread (&c2, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		fread (&c1, 1, 1, in);	/* loop size */
		fread (&c2, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* pattern table lenght */
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , c1 ); */

	/* NoiseTracker restart byte */
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);

	max = 0x00;
	for (i = 0; i < 128; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		if (c1 > max)
			max = c1;
	}
	/*printf ( "Number of pattern : %d\n" , max+1 ); */

	c1 = 'M';
	c2 = '.';
	c3 = 'K';

	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* bypass 4 unknown empty bytes */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	if ((c1 != 0x00) || (c2 != 0x00) || (c3 != 0x00) || (c4 != 0x00)) {
		fseek (in, -4, 1);	/* SEEK_CUR */
	}
	/*else */
	/*printf ( "! four empty bytes bypassed at the beginning of the pattern data\n" ); */

	/* pattern data */
	t = (uint8 *) malloc ((max + 1) * 1024);
	bzero (t, (max + 1) * 1024);
	fread (t, (max + 1) * 1024, 1, in);
	fwrite (t, (max + 1) * 1024, 1, out);
	free (t);

	/* sample data */
	t = (uint8 *) malloc (ssize);
	fread (t, ssize, 1, in);
	fwrite (t, ssize, 1, out);
	free (t);

	return 0;
}


static int test_MP_noID (uint8 *data, int s)
{
	int start, ssize;
	int j, k, l, m, n;

	start = 0;

#if 0
	if (i < 3) {
		Test = BAD;
		return;
	}
#endif

	/* test #2 */
	l = 0;
	for (j = 0; j < 31; j++) {
		/* size */
		k = (((data[start + 8 * j] << 8) +
			 data[start + 1 + 8 * j]) * 2);
		/* loop start */
		m = (((data[start + 4 + 8 * j] << 8) +
			 data[start + 5 + 8 * j]) * 2);
		/* loop size */
		n = (((data[start + 6 + 8 * j] << 8) +
			 data[start + 7 + 8 * j]) * 2);
		l += k;

		/* finetune > 0x0f ? */
		if (data[start + 2 + 8 * j] > 0x0f)
			return -1;

		/* loop start+replen > size ? */
		if (n != 2 && (m + n) > k)
			return -1;

		/* loop size > size ? */
		if (n > (k + 2))
			return -1;

		/* loop start != 0 and loop size = 0 */
		if (m != 0 && n <= 2)
			return -1;

		/* when size!=0  loopsize==0 ? */
		if (k != 0 && n == 0)
			return -1;
	}

	if (l <= 2)
		return -1;

	/* test #3 */
	l = data[start + 248];
	if (l > 0x7f || l == 0x00)
		return -1;

	/* test #4 */
	/* l contains the size of the pattern list */
	k = 0;
	for (j = 0; j < 128; j++) {
		if (data[start + 250 + j] > k)
			k = data[start + 250 + j];
		if (data[start + 250 + j] > 0x7f)
			return -1;
		if (j > l + 3) {
			if (data[start + 250 + j] != 0x00)
				return -1;
		}
	}
	k++;

	/* test #5  ptk notes .. gosh ! (testing all patterns !) */
	/* k contains the number of pattern saved */
	for (j = 0; j < (256 * k); j++) {
		l = data[start + 378 + j * 4];
		if (l > 19)
			return -1;

		ssize = data[start + 378 + j * 4] & 0x0f;
		ssize *= 256;
		ssize += data[start + 379 + j * 4];

		if (ssize > 0 && ssize < 0x71)
			return -1;
	}

	/* test #6  (loopStart+LoopSize > Sample ? ) */
	for (j = 0; j < 31; j++) {
		k = (((data[start + j * 8] << 8) +
			 data[start + 1 + j * 8]) * 2);
		l = (((data[start + 4 + j * 8] << 8) +
			 data[start + 5 + j * 8]) * 2) +
			(((data[start + 6 + j * 8] << 8) +
			data[start + 7 + j * 8]) * 2);
		if (l > (k + 2))
			return -1;
	}

	return 0;
}


static int test_MP_ID (uint8 *data, int s)
{
	int j, l, k;
	int start = 0;

	/* "TRK1" Module Protector */
	if (data[0]!='T' || data[1]!='R' || data[2]!='K' || data[3]!='1')
		return -1;

	/* test #1 */
	for (j = 0; j < 31; j++) {
		if (data[start + 6 + 8 * j] > 0x0f)
			return -1;
	}

	/* test #2 */
	l = data[start + 252];
	if (l > 0x7f || l == 0x00)
		return -1;

	/* test #4 */
	k = 0;
	for (j = 0; j < 128; j++) {
		if (data[start + 254 + j] > k)
			k = data[start + 254 + j];
		if (data[start + 254 + j] > 0x7f)
			return -1;
	}
	k++;

	/* test #5  ptk notes .. gosh ! (testing all patterns !) */
	/* k contains the number of pattern saved */
	for (j = 0; j < (256 * k); j++) {
		l = data[start + 382 + j * 4];
		if (l > 19)
			return -1;
	}

	return 0;
}
