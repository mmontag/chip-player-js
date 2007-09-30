/*
 * Module_Protector.c   Copyright (C) 1997 Asle / ReDoX
 *                      Copyright (C) 2006-2007 Claudio Matsuoka
 *
 * Converts MP packed MODs back to PTK MODs
 *
 * $Id: mp.c,v 1.6 2007-09-30 00:08:19 cmatsuoka Exp $
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
	depack_MP
};

struct pw_format pw_mp_noid = {
	"MP_noid",
	"Module Protector",
	0x00,
	test_MP_noID,
	depack_MP
};


static int depack_MP (FILE *in, FILE *out)
{
	uint8 c1;
	uint8 ptable[128];
	uint8 max;
	uint8 *t;
	int i, j;
	int size, ssize = 0;

	bzero(ptable, 128);

	for (i = 0; i < 20; i++)			/* title */
		write8(out, 0);

	if (read32b(in) != 0x54524B31)			/* TRK1 */
		fseek(in, -4, SEEK_CUR);

	for (i = 0; i < 31; i++) {
		for (j = 0; j < 22; j++)		/*sample name */
			write8(out, 0);

		write16b(out, size = read16b(in));	/* size */
		ssize += size * 2;
		write8(out, read8(in));			/* finetune */
		write8(out, read8(in));			/* volume */
		write16b(out, read16b(in));		/* loop start */
		write16b(out, read16b(in));		/* loop size */
	}

	write16b(out, read16b(in));		/* pattern table length */
	write16b(out, read16b(in));		/* NoiseTracker restart byte */

	for (max = i = 0; i < 128; i++) {
		write8(out, c1 = read8(in));
		if (c1 > max)
			max = c1;
	}

	write32b(out, PW_MOD_MAGIC);		/* M.K. */

	if (read32b(in) != 0)			/* bypass unknown empty bytes */
		fseek (in, -4, SEEK_CUR);

	/* pattern data */
	t = (uint8 *)malloc((max + 1) * 1024);
	bzero(t, (max + 1) * 1024);
	fread(t, (max + 1) * 1024, 1, in);
	fwrite(t, (max + 1) * 1024, 1, out);
	free(t);

	/* sample data */
	t = (uint8 *)malloc(ssize);
	fread(t, ssize, 1, in);
	fwrite(t, ssize, 1, out);
	free(t);

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
		int x = start + 8 * j;

		k = readmem16b(data + x) * 2;		/* size */
		m = readmem16b(data + x + 4) * 2;	/* loop start */
		n = readmem16b(data + x + 6) * 2;	/* loop size */
		l += k;

		/* finetune > 0x0f ? */
		if (data[x + 2] > 0x0f)
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
		int x = start + j * 4;

		l = data[x + 378];
		if (l > 19 && l != 74)		/* MadeInCroatia has l == 74 */
			return -1;

		ssize = data[x + 378] & 0x0f;
		ssize *= 256;
		ssize += data[x + 379];

		if (ssize > 0 && ssize < 0x71)
			return -1;
	}

	/* test #6  (loopStart+LoopSize > Sample ? ) */
	for (j = 0; j < 31; j++) {
		int x = start + j * 8;

		k = readmem16b(data + x) * 2;
		l = (readmem16b(data + x + 4) + readmem16b(data + x + 6)) * 2;

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
	if (readmem32b(data) != 0x54524B31)
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
