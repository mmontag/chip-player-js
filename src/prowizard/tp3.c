/*
 *   TrackerPacker_v3.c   Copyright (C) 1998 Asle / ReDoX
 *                        Copyright (C) 2007 Claudio Matsuoka
 *
 * Converts tp3 packed MODs back to PTK MODs
 *
 * $Id: tp3.c,v 1.2 2007-09-24 18:30:29 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int depack_tp3 (uint8 *, FILE *);
static int test_tp3 (uint8 *, int);

struct pw_format pw_tp3 = {
        "TP3",
        "Trackerpacker 3",
        0x00,
        test_tp3,
        depack_tp3,
        NULL
};


static int depack_tp3 (uint8 *data, FILE *out)
{
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 ptk_table[37][2];
	uint8 pnum[128];
	uint8 Pattern[1024];
	uint8 *tmp;
	uint8 note, ins, fxt, fxp;
	uint8 PatMax = 0x00;
	uint8 PatPos;
	int Track_Address[128][4];
	int i = 0, j = 0, k;
	int Start_Pat_Address = 999999l;
	int Whole_Sample_Size = 0;
	int Max_Track_Address = 0;
	int start = 0;
	int Where = start;

	bzero(Track_Address, 128 * 4 * 4);
	bzero(pnum, 128);

	/* title */
	Where += 8;
	fwrite (&data[Where], 20, 1, out);
	Where += 20;

	/* number of sample */
	c1 = data[Where++];
	c2 = data[Where++];
	j = (c1 << 8) + c2;
	j /= 8;

	for (i = 0; i < j; i++) {
		c1 = 0x00;
		for (k = 0; k < 22; k++)	/*sample name */
			fwrite (&c1, 1, 1, out);

		/* read fine */
		c3 = data[Where++];

		/* read volume */
		c4 = data[Where++];

		/* size */
		c1 = data[Where++];
		c2 = data[Where++];
		Whole_Sample_Size += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		/* write finetune */
		fwrite (&c3, 1, 1, out);

		/* write volume */
		fwrite (&c4, 1, 1, out);

		c1 = data[Where++];	/* loop start */
		c2 = data[Where++];
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		c1 = data[Where++];	/* loop size */
		c2 = data[Where++];
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

	}
	tmp = (uint8 *) malloc (30);
	bzero (tmp, 30);
	tmp[29] = 0x01;
	while (i != 31) {
		fwrite (tmp, 30, 1, out);
		i++;
	}
	/*printf ( "Whole sample size : %ld\n" , Whole_Sample_Size ); */

	/* read size of pattern table */
	c1 = data[Where++];
	c2 = data[Where++];
	PatPos = c2;
	fwrite (&PatPos, 1, 1, out);

	/* ntk byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	for (i = 0; i < PatPos; i++) {
		c3 = data[Where++];
		c4 = data[Where++];
		pnum[i] = ((c3 << 8) + c4) / 8;
		if (pnum[i] > PatMax)
			PatMax = pnum[i];
/*fprintf ( info , "%3ld: %ld\n" , i,paddr[i] );*/
	}

	/* read tracks addresses */
	/* bypass 4 bytes or not ?!? */
	/* Here, I choose not :) */
/*fprintf ( info , "track addresses :\n" );*/
	for (i = 0; i <= PatMax; i++) {
		for (j = 0; j < 4; j++) {
			c1 = data[Where++];
			c2 = data[Where++];
			Track_Address[i][j] = (c1 << 8) + c2;
			if (Track_Address[i][j] > Max_Track_Address)
				Max_Track_Address = Track_Address[i][j];
/*fprintf ( info , "%6ld, " , Track_Address[i][j] );*/
		}
/*fprintf ( info , "  (%x)\n" , Max_Track_Address );*/
	}

	/*printf ( "Highest pattern number : %d\n" , PatMax ); */

	/* write pattern list */
	fwrite (pnum, 128, 1, out);

	/* ID string */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	Start_Pat_Address = Where + 2;
	/*printf ( "address of the first pattern : %ld\n" , Start_Pat_Address ); */
/*fprintf ( info , "address of the first pattern : %x\n" , Start_Pat_Address );*/

	/* pattern datas */
	/*printf ( "converting pattern data " ); */
	for (i = 0; i <= PatMax; i++) {
/*fprintf ( info , "\npattern %ld:\n\n" , i );*/
		bzero (Pattern, 1024);
		for (j = 0; j < 4; j++) {
/*fprintf ( info , "track %ld: (at %ld)\n" , j , Track_Address[i][j]+Start_Pat_Address );*/
			Where = start + Track_Address[i][j] +
				Start_Pat_Address;
			for (k = 0; k < 64; k++) {
				c1 = data[Where++];
/*fprintf ( info , "%ld: %2x," , k , c1 );*/
				if ((c1 & 0xC0) == 0xC0) {
/*fprintf ( info , " <--- %d empty lines\n" , (0x100-c1) );*/
					k += (0x100 - c1);
					k -= 1;
					continue;
				}
				if ((c1 & 0xC0) == 0x80) {
					c2 = data[Where++];
/*fprintf ( info , "%2x ,\n" , c2 );*/
					fxt = (c1 >> 1) & 0x0f;
					fxp = c2;
					if ((fxt == 0x05) || (fxt == 0x06)
						|| (fxt == 0x0A)) {
						if (fxp > 0x80)
							fxp = 0x100 - fxp;
						else if (fxp <= 0x80)
							fxp =
								(fxp << 4) &
								0xf0;
					}
					if (fxt == 0x08)
						fxt = 0x00;
					Pattern[k * 16 + j * 4 + 2] = fxt;
					Pattern[k * 16 + j * 4 + 3] = fxp;
					continue;
				}

				c2 = data[Where++];
/*fprintf ( info , "%2x, " , c2 );*/
				ins = ((c2 >> 4) & 0x0f) | ((c1 >> 2) & 0x10);
				if ((c1 & 0x40) == 0x40)
					note = 0x7f - c1;
				else
					note = c1 & 0x3F;
				fxt = c2 & 0x0F;
				if (fxt == 0x00) {
/*fprintf ( info , " <--- No FX !!\n" );*/
					Pattern[k * 16 + j * 4] = ins & 0xf0;
					Pattern[k * 16 + j * 4] |=
						ptk_table[note][0];
					Pattern[k * 16 + j * 4 + 1] =
						ptk_table[note][1];
					Pattern[k * 16 + j * 4 + 2] =
						(ins << 4) & 0xf0;
					Pattern[k * 16 + j * 4 + 2] |= fxt;
					continue;
				}
				c3 = data[Where++];
/*fprintf ( info , "%2x\n" , c3 );*/
				if (fxt == 0x08)
					fxt = 0x00;
				fxp = c3;
				if ((fxt == 0x05) || (fxt == 0x06)
					|| (fxt == 0x0A)) {
					if (fxp > 0x80)
						fxp = 0x100 - fxp;
					else if (fxp <= 0x80)
						fxp = (fxp << 4) & 0xf0;
				}

				Pattern[k * 16 + j * 4] = ins & 0xf0;
				Pattern[k * 16 + j * 4] |= ptk_table[note][0];
				Pattern[k * 16 + j * 4 + 1] = ptk_table[note][1];
				Pattern[k * 16 + j * 4 + 2] =
					(ins << 4) & 0xf0;
				Pattern[k * 16 + j * 4 + 2] |= fxt;
				Pattern[k * 16 + j * 4 + 3] = fxp;
			}
			if (Where > Max_Track_Address)
				Max_Track_Address = Where;
/*fprintf ( info , "%6ld, " , Max_Track_Address );*/
		}
		fwrite (Pattern, 1024, 1, out);
		/*printf ( "." ); */
	}
	/*printf ( " ok\n" ); */

	/*printf ( "sample data address : %ld\n" , Max_Track_Address ); */

	/* Sample data */
	if (((Max_Track_Address / 2) * 2) != Max_Track_Address)
		Max_Track_Address += 1;
	fwrite (&data[start + Max_Track_Address],
		Whole_Sample_Size, 1, out);

	return 0;
}


static int test_tp3(uint8 *data, int s)
{
	int start = 0;
	int j, k, l, m, n;
	int ssize;

	PW_REQUEST_DATA(s, 1024);

	if (memcmp(data, "CPLX_TP3", 8))
		return -1;

	/* number of sample */
	l = ((data[start + 28] << 8) + data[start + 29]);

	if ((((l / 8) * 8) != l) || (l == 0))
		return -1;

	l /= 8;

	/* l is the number of sample */

	/* test finetunes */
	for (k = 0; k < l; k++) {
		if (data[start + 30 + k * 8] > 0x0f)
			return -1;
	}

	/* test volumes */
	for (k = 0; k < l; k++) {
		if (data[start + 31 + k * 8] > 0x40)
			return - 1;
	}

	/* test sample sizes */
	ssize = 0;
	for (k = 0; k < l; k++) {
		/* size */
		j = (data[start + k * 8 + 32] << 8) + data[start + k * 8 + 33];
		/* loop start */
		m = (data[start + k * 8 + 34] << 8) + data[start + k * 8 + 35];
		/* loop size */
		n = (data[start + k * 8 + 36] << 8) + data[start + k * 8 + 37];
		j *= 2;
		m *= 2;
		n *= 2;
		if ((j > 0xFFFF) || (m > 0xFFFF) || (n > 0xFFFF))
			return -1;

		if ((m + n) > (j + 2))
			return -1;

		if ((m != 0) && (n == 0))
			return -1;

		ssize += j;
	}

	if (ssize <= 4)
		return -1;

	/* pattern list size */
	j = data[start + l * 8 + 31];
	if ((l == 0) || (l > 128))
		return -1;

	/* j is the size of the pattern list */
	/* l is the number of sample */
	/* ssize is the sample data size */

	return 0;
}
