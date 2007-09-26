/*
 * Unic_Tracker_2.c   Copyright (C) 1997 Asle / ReDoX
 *                    Copyright 2006-2007 Claudio Matsuoka
 *
 * 
 * Unic tracked 2 MODs to Protracker
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 */

/*
 * $Id: unic2.c,v 1.4 2007-09-26 03:12:11 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_unic2 (uint8 *, int);
static int depack_unic2 (uint8 *, FILE *);

struct pw_format pw_unic2 = {
	"UNIC2",
	"Unic Tracker 2",
	0x00,
	test_unic2,
	depack_unic2,
	NULL
};


#define ON 1
#define OFF 2

static int depack_unic2 (uint8 *data, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 npat = 0x00;
	uint8 maxpat = 0x00;
	uint8 ins, note, fxt, fxp;
	uint8 fine = 0x00;
	uint8 pat[1025];
	uint8 LOOP_START_STATUS = OFF;	/* standard /2 */
	int i = 0, j = 0, k = 0, l = 0;
	int ssize = 0;
	int start = 0, w = 0;

	/* title */
	for (i = 0; i < 20; i++)
		write8(out, 0);

	for (i = 0; i < 31; i++) {
		/* sample name */
		fwrite(data + w, 20, 1, out);
		w += 20;
		write8(out, 0);
		write8(out, 0);

		/* fine on ? */
		c1 = data[w++];
		c2 = data[w++];
		j = (c1 << 8) + c2;
		if (j != 0) {
			if (j < 256)
				fine = 0x10 - c2;
			else
				fine = 0x100 - c2;
		} else
			fine = 0x00;

		/* smp size */
		write16b(out, l = readmem16b(data + w));
		w += 2;
		ssize += l * 2;

		w += 1;
		write8(out, fine);		/* fine */
		write8(out, data[w++]);		/* vol */

		j = readmem16b(data + w);	/* loop start */
		w += 2;
		k = readmem16b(data + w);	/* loop size */
		w += 2;

		if ((((j * 2) + k) <= l) && (j != 0)) {
			LOOP_START_STATUS = ON;
			j *= 2;
		}

		write16b(out, j);
		write16b(out, k);
	}

	write8(out, npat = data[w++]);		/* number of pattern */
	write8(out, 0x7f);			/* noisetracker byte */
	w++;

	fwrite(&data[w], 128, 1, out);		/* pat table */
	w += 128;

	/* get highest pattern number */
	for (maxpat = i = 0; i < 128; i++) {
		if (data[start + 932 + i] > maxpat)
			maxpat = data[start + 932 + i];
	}
	maxpat += 1;		/* coz first is $00 */
	/*printf ( "Number of pat : %d\n" , maxpat ); */

	write32b(out, PW_MOD_MAGIC);

	/* pattern data */
	for (i = 0; i < maxpat; i++) {
		for (j = 0; j < 256; j++) {
			int x = w + j * 3;

			ins = ((data[x] >> 2) & 0x10) |
					((data[x + 1] >> 4) & 0x0f);
			note = data[x] & 0x3f;
			fxt = data[x + 1] & 0x0f;
			fxp = data[x + 2];
			if (fxt == 0x0d) {	/* pattern break */
				c4 = fxp % 10;
				c3 = fxp / 10;
				fxp = 16;
				fxp *= c3;
				fxp += c4;
			}

			pat[j * 4] = (ins & 0xf0);
			pat[j * 4] |= ptk_table[note][0];
			pat[j * 4 + 1] = ptk_table[note][1];
			pat[j * 4 + 2] = ((ins << 4) & 0xf0) | fxt;
			pat[j * 4 + 3] = fxp;
		}
		fwrite(pat, 1024, 1, out);
		w += 768;
	}

	/* sample data */
	fwrite(&data[w], ssize, 1, out);

	return 0;
}


static int test_unic2 (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	/* test 1 */
	PW_REQUEST_DATA (s, 1084);

	/* test #2 ID = $00000000 ? */
	if (data[start + 1080] == 00 && data[start + 1081] == 00 &&
		data[start + 1082] == 00 && data[start + 1083] == 00)
		return -1;

	/* test 2,5 :) */
	o = 0;
	ssize = 0;
	for (k = 0; k < 31; k++) {
		j = ((data[start + 22 + k * 30] * 256 +
			 data[start + 23 + k * 30]) * 2);
		m = ((data[start + 26 + k * 30] * 256 +
			 data[start + 27 + k * 30]) * 2);
		n = ((data[start + 28 + k * 30] * 256 +
			 data[start + 29 + k * 30]) * 2);
		ssize += j;

		if (j + 2 < m + n)
			return -1;

		if (j > 0xffff || m > 0xffff || n > 0xffff)
			return -1;

		if (data[start + 25 + k * 30] > 0x40)
			return -1;

		if (data[start + 20 + k * 30] * 256 + data[start + 21 + k * 30]
			&& j == 0)
			return -1;

		if (data[start + 25 + k * 30] != 0 && j == 0)
			return -1;

		/* get the highest !0 sample */
		if (j != 0)
			o = j + 1;
	}

	if (ssize <= 2)
		return -1;

	/* test #4  pattern list size */
	l = data[start + 930];
	if (l > 127 || l == 0)
		return -1;
	/* l holds the size of the pattern list */

	k = 0;
	for (j = 0; j < l; j++) {
		if (data[start + 932 + j] > k)
			k = data[start + 932 + j];
		if (data[start + 932 + j] > 127)
			return -1;
	}
	/* k holds the highest pattern number */

	/* test last patterns of the pattern list = 0 ? */
	j += 2;		/* just to be sure .. */
	while (j != 128) {
		if (data[start + 932 + j] != 0)
			return -1;
		j += 1;
	}
	/* k is the number of pattern in the file (-1) */
	k += 1;

#if 0
	/* test #5 pattern data ... */
	if (((k * 768) + 1060 + start) > in_size) {
/*printf ( "#5,0 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
#endif

	PW_REQUEST_DATA (s, 1060 + k * 256 * 3 + 2);

	for (j = 0; j < (k << 8); j++) {
		/* relative note number + last bit of sample > $34 ? */
		if (data[start + 1060 + j * 3] > 0x74)
			return -1;
		if ((data[start + 1060 + j * 3] & 0x3F) > 0x24)
			return -1;
		if ((data[start + 1060 + j * 3 + 1] & 0x0F) == 0x0C
			&& data[start + 1060 + j * 3 + 2] > 0x40)
			return -1;
		if ((data[start + 1060 + j * 3 + 1] & 0x0F) == 0x0B
			&& data[start + 1060 + j * 3 + 2] > 0x7F)
			return -1;
		if ((data[start + 1060 + j * 3 + 1] & 0x0F) == 0x0D
			&& data[start + 1060 + j * 3 + 2] > 0x40)
			return -1;

		n = ((data[start + 1060 + j * 3] >> 2) & 0x30) |
			((data[start + 1061 + j * 3 + 1] >> 4) & 0x0F);
		if (n > o)
			return -1;
	}

	return 0;
}
