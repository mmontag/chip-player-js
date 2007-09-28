/*
 * Unic_Tracker.c   Copyright (C) 1997 Asle / ReDoX
 *		    Copyright (C) 2006-2007 Claudio Matsuoka
 * 
 * Unic tracked MODs to Protracker
 * both with or without ID Unic files will be converted
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 */

/*
 * $Id: unic.c,v 1.10 2007-09-28 13:07:40 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_unic_id (uint8 *, int);
static int test_unic_noid (uint8 *, int);
static int test_unic_emptyid (uint8 *, int);
static int depack_unic (uint8 *, FILE *);

struct pw_format pw_unic_id = {
	"UNIC",
	"UNIC Tracker",
	0x00,
	test_unic_id,
	depack_unic,
	NULL
};

struct pw_format pw_unic_noid = {
	"UNICn",
	"UNIC Tracker noid",
	0x00,
	test_unic_noid,
	depack_unic,
	NULL
};

struct pw_format pw_unic_emptyid = {
	"UNIC0",
	"UNIC Tracker id0",
	0x00,
	test_unic_emptyid,
	depack_unic,
	NULL
};


#define ON 1
#define OFF 2

static int depack_unic (uint8 *data, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 npat;
	uint8 max = 0;
	uint8 ins, note, fxt, fxp;
	uint8 fine;
	uint8 pat[1025];
	uint8 loop_status = OFF;	/* standard /2 */
	int i = 0, j = 0, k = 0, l = 0;
	int ssize = 0;
	int w = 0;		/* main pointer to prevent fread() */
	int start = 0;

	/* title */
	fwrite(&data[w], 20, 1, out);
	w += 20;

	for (i = 0; i < 31; i++) {
		/* sample name */
		fwrite(&data[w], 20, 1, out);
		write8(out, 0);
		write8(out, 0);
		w += 20;

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
			fine = 0;

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
			loop_status = ON;
			j *= 2;
		}

		write16b(out, j);
		write16b(out, k);
	}


/*  printf ( "whole sample size : %ld\n" , ssize );*/
/*
  if ( loop_status == ON )
    printf ( "!! Loop start value was /4 !\n" );
*/
	write8(out, npat = data[w++]);		/* number of pattern */
	write8(out, 0x7f);			/* noisetracker byte */
	w += 1;

	/* pat table */
	fwrite(&data[w], 128, 1, out);
	w += 128;

	/* get highest pattern number */
	for (i = 0; i < 128; i++) {
		if (data[start + 952 + i] > max)
			max = data[start + 952 + i];
	}
	max += 1;		/* coz first is $00 */

	write32b(out, PW_MOD_MAGIC);

	/* verify UNIC ID */
	w = start + 1080;
	if ((strncmp((char *)&data[w], "M.K.", 4) == 0) ||
		(strncmp((char *)&data[w], "UNIC", 4) == 0) ||
		((data[w] == 0x00) && (data[w + 1] == 0x00)
			&& (data[w + 2] == 0x00)
			&& (data[w + 3] == 0x00)))
		w = start + 1084l;
	else
		w = start + 1080l;


	/* pattern data */
	for (i = 0; i < max; i++) {
		for (j = 0; j < 256; j++) {
			ins = ((data[w + j * 3] >> 2) & 0x10) |
				((data[w + j * 3 + 1] >> 4) & 0x0f);
			note = data[w + j * 3] & 0x3f;
			fxt = data[w + j * 3 + 1] & 0x0f;
			fxp = data[w + j * 3 + 2];

			if (fxt == 0x0d) {	/* pattern break */
/*        printf ( "!! [%x] -> " , fxp );*/
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


static int test_unic_id (uint8 *data, int s)
{
	int j, k, l, n;
	int start = 0, ssize;

	/* test 1 */
	PW_REQUEST_DATA (s, 1084);

	if (data[start + 1080] != 'M' || data[start + 1081] != '.' ||
		data[start + 1082] != 'K' || data[start + 1083] != '.')
		return -1;

	/* test 2 */
	ssize = 0;
	for (k = 0; k < 31; k++) {
		int x = start + k * 30;

		j = (((data[x + 42] << 8) + data[x + 43]) * 2);
		ssize += j;
		n = (((data[x + 46] << 8) + data[x + 47]) * 2) +
			(((data[x + 48] << 8) + data[x + 49]) * 2);
		if ((j + 2) < n)
			return -1;
	}

	if (ssize <= 2)
		return -1;

	/* test #3  finetunes & volumes */
	for (k = 0; k < 31; k++) {
		int x = start + k * 30;

		if (data[x + 44] > 0x0f || data[x + 45] > 0x40)
			return -1;
	}

	/* test #4  pattern list size */
	l = data[start + 950];
	if (l > 127 || l == 0)
		return -1;
	/* l holds the size of the pattern list */

	k = 0;
	for (j = 0; j < l; j++) {
		if (data[start + 952 + j] > k)
			k = data[start + 952 + j];
		if (data[start + 952 + j] > 127)
			return -1;
	}
	/* k holds the highest pattern number */

	/* test last patterns of the pattern list = 0 ? */
	while (j != 128) {
		if (data[start + 952 + j] != 0)
			return -1;
		j += 1;
	}
	/* k is the number of pattern in the file (-1) */
	k += 1;

	PW_REQUEST_DATA (s, 1084 + k * 256 * 3);

#if 0
	/* test #5 pattern data ... */
	if (((k * 768) + 1084 + start) > in_size)
		return -1;
#endif

	for (j = 0; j < (k << 8); j++) {
		/* relative note number + last bit of sample > $34 ? */
		if (data[start + 1084 + j * 3] > 0x74)
			return -1;
	}

	return 0;
}


static int test_unic_emptyid (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	/* test 1 */
	PW_REQUEST_DATA (s, 1084);

	/* test #2 ID = $00000000 ? */
	if (data[start + 1080] != 00 || data[start + 1081] != 00 ||
		data[start + 1082] != 00 || data[start + 1083] != 00)
		return -1;

	/* test 2,5 :) */
	ssize = 0;
	o = 0;
	for (k = 0; k < 31; k++) {
		int x = start + k * 30;

		j = (((data[x + 42] << 8) + data[x + 43]) * 2);
		m = (((data[x + 46] << 8) + data[x + 47]) * 2);
		n = (((data[x + 48] << 8) + data[x + 49]) * 2);
		ssize += j;

		if (n != 0 && (j + 2) < (m + n))
			return -1;

		if (j > 0xffff || m > 0xffff || n > 0xffff)
			return -1;

		if (data[x + 45] > 0x40)
			return -1;

		/* finetune ... */
		if ((((data[x + 40] << 8) + data[x + 41]) != 0 && j == 0)
			|| ((data[x + 40] * 256 + data[x + 41] > 8)
			&& (data[x + 40] * 256 + data[x + 41]) < 247))
			return -1;

		/* loop start but no replen ? */
		if (m != 0 && n <= 2)
			return -1;

		if (data[x + 45] != 0 && j == 0)
			return -1;

		/* get the highest !0 sample */
		if (j != 0)
			o = j + 1;
	}

	if (ssize <= 2)
		return -1;

	/* test #4  pattern list size */
	l = data[start + 950];
	if (l > 127 || l == 0)
		return -1;
	/* l holds the size of the pattern list */

	k = 0;
	for (j = 0; j < l; j++) {
		if (data[start + 952 + j] > k)
			k = data[start + 952 + j];
		if (data[start + 952 + j] > 127)
			return -1;
	}
	/* k holds the highest pattern number */

	/* test last patterns of the pattern list = 0 ? */
	while (j != 128) {
		if (data[start + 952 + j] != 0)
			return -1;
		j += 1;
	}
	/* k is the number of pattern in the file (-1) */
	k += 1;

#if 0
	/* test #5 pattern data ... */
	if ((k * 768 + 1084 + start) > in_size)
		return -1;
#endif

	PW_REQUEST_DATA (s, 1084 + k * 256 * 3 + 2);

	for (j = 0; j < (k << 8); j++) {
		int y = start + 1084 + j * 3;

		/* relative note number + last bit of sample > $34 ? */
		if (data[y] > 0x74)
			return -1;

		if ((data[y] & 0x3F) > 0x24)
			return -1;

		if ((data[y + 1] & 0x0F) == 0x0C
			&& data[y + 2] > 0x40)
			return -1;

		if ((data[y + 1] & 0x0F) == 0x0B
			&& data[y + 2] > 0x7F)
			return -1;

		if ((data[y + 1] & 0x0F) == 0x0D
			&& data[y + 2] > 0x40)
			return -1;

		n = ((data[y] >> 2) & 0x30) |
			((data[start + 1085 + j * 3 + 1] >> 4) & 0x0F);

		if (n > o)
			return -1;
	}

	return 0;
}


static int test_unic_noid (uint8 *data, int s)
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
	ssize = 0;
	o = 0;
	for (k = 0; k < 31; k++) {
		int x = start + k * 30;

		j = (((data[x + 42] << 8) + data[x + 43]) * 2);
		m = (((data[x + 46] << 8) + data[x + 47]) * 2);
		n = (((data[x + 48] << 8) + data[x + 49]) * 2);

		ssize += j;
		if (n != 0 && (j + 2) < (m + n))
			return -1;

		/* samples too big ? */
		if (j > 0xffff || m > 0xffff || n > 0xffff)
			return -1;

		/* volume too big */
		if (data[x + 45] > 0x40)
			return -1;

		/* finetune ... */
		if (((data[x + 40] * 256 + data[x + 41]) != 0 && j == 0)
			|| ((data[x + 40] * 256 + data[x + 41]) > 8 &&
			data[x + 40] * 256 + data[x + 41] < 247))
			return -1;

		/* loop start but no replen ? */
		if (m != 0 && n <= 2)
			return -1;

		if (data[x + 45] != 0 && j == 0)
			return -1;

		/* get the highest !0 sample */
		if (j != 0)
			o = j + 1;
	}
	if (ssize <= 2)
		return -1;

	/* test #4  pattern list size */
	l = data[start + 950];
	if (l > 127 || l == 0)
		return -1;
	/* l holds the size of the pattern list */

	k = 0;
	for (j = 0; j < l; j++) {
		if (data[start + 952 + j] > k)
			k = data[start + 952 + j];
		if (data[start + 952 + j] > 127)
			return -1;
	}
	/* k holds the highest pattern number */

	/* test last patterns of the pattern list = 0 ? */
	while (j != 128) {
		if (data[start + 952 + j] != 0)
			return -1;
		j += 1;
	}
	/* k is the number of pattern in the file (-1) */
	k += 1;

	/* test #5 pattern data ... */
	/* o is the highest !0 sample */

#if 0
	if (((k * 768) + 1080 + start) > in_size) {
		Test = BAD;
		return;
	}
#endif

	PW_REQUEST_DATA (s, 1080 + k * 256 * 3 + 2);

	for (j = 0; j < (k << 8); j++) {
		int y = start + 1080 + j * 3;

		/* relative note number + last bit of sample > $34 ? */
		if (data[y] > 0x74)
			return -1;
		if ((data[y] & 0x3F) > 0x24)
			return -1;
		if ((data[y + 1] & 0x0F) == 0x0C && data[y + 2] > 0x40)
			return -1;

		if ((data[y + 1] & 0x0F) == 0x0B && data[y + 2] > 0x7F)
			return -1;

		if ((data[y + 1] & 0x0F) == 0x0D && data[y + 2] > 0x40)
			return -1;

		n = ((data[y] >> 2) & 0x30) |
			((data[start + 1081 + j * 3 + 1] >> 4) & 0x0F);

		if (n > o)
			return -1;
	}

	/* test #6  title coherent ? */
	for (j = 0; j < 20; j++) {
		if ((data[start + j] != 0 && data[start + j] < 32) ||
			data[start + j] > 180)
			return -1;
	}

	return 0;
}
