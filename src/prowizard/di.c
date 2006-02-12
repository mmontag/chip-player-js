/*
 * Digital_Illusion.c   Copyright (C) 1997 Asle / ReDoX
 *			Modified by Claudio Matsuoka
 *
 * Converts DI packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * $Id: di.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_DI (uint8 *, int);
static int depack_DI (FILE *, FILE *);

struct pw_format pw_di = {
	"DI",
	"Digital Illusion",
	0x00,
	test_DI,
	NULL,
	depack_DI
};


static int depack_DI (FILE * in, FILE * out)
{
	uint8 c1, c2, c3, c4;
	uint8 note, ins, fxt, fxp;
	uint8 ptk_tab[5];
	uint8 npat = 0x00;
	uint8 ptable[128];
	uint8 Max = 0x00;
	uint8 *t;
	long i = 0, k = 0;
	uint16 paddr[128];
	short nins = 0;
	long Add_Pattern_Table = 0;
	long Add_Pattern_Data = 0;
	long Add_Sample_Data = 0;
	long tmp_long;
	long ssize = 0;

	bzero (ptable, 128);
	bzero (ptk_tab, 5);
	bzero (paddr, 128);

	/* title */
	t = (uint8 *) malloc (20);
	bzero (t, 20);
	fwrite (t, 20, 1, out);
	free (t);

	fseek (in, 0, 0);
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	nins = (c1 << 8) + c2;
	/*printf ( "Number of sample : %d\n" , nins ); */

	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Add_Pattern_Table = ((c2 << 8) << 8) + (c3 << 8) + c4;
	/*printf ( "Pattern table address : %ld\n" , Add_Pattern_Table ); */

	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Add_Pattern_Data = ((c2 << 8) << 8) + (c3 << 8) + c4;
	/*printf ( "Pattern data address : %ld\n" , Add_Pattern_Data ); */

	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Add_Sample_Data = ((c2 << 8) << 8) + (c3 << 8) + c4;
	/*printf ( "Sample data address : %ld\n" , Add_Sample_Data ); */


	t = (uint8 *) malloc (22);
	bzero (t, 22);
	for (i = 0; i < nins; i++) {
		fwrite (t, 22, 1, out);

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

	c1 = 0x00;
	c2 = 0x01;
	for (i = nins; i < 31; i++) {
		fwrite (t, 22, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	free (t);

	tmp_long = ftell (in);

	fseek (in, Add_Pattern_Table, 0);
	i = 0;
	do {
		fread (&c1, 1, 1, in);
		ptable[i] = c1;
		i += 1;
	} while (c1 != 0xff);
	ptable[i - 1] = 0x00;
	npat = i - 1;
	fwrite (&npat, 1, 1, out);

	c2 = 0x7f;
	fwrite (&c2, 1, 1, out);

	Max = 0;
	for (i = 0; i < 128; i++) {
		fwrite (&ptable[i], 1, 1, out);
		if (ptable[i] > Max)
			Max = ptable[i];
	}

	/*printf ( "Number of pattern : %d\n" , npat ); */
	/*printf ( "Highest pattern number : %d\n" , Max ); */

	c1 = 'M';
	c2 = '.';
	c3 = 'K';

	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);


	fseek (in, tmp_long, 0);
	for (i = 0; i <= Max; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		paddr[i] = (c1 << 8) + c2;
	}

	for (i = 0; i <= Max; i++) {
		fseek (in, paddr[i], 0);
		for (k = 0; k < 256; k++) {	/* 256 = 4 voices * 64 rows */
			bzero (ptk_tab, 5);
			fread (&c1, 1, 1, in);
			if ((c1 & 0x80) == 0) {
				fread (&c2, 1, 1, in);
				note =
					(((c1 << 4) & 0x30) | ((c2 >> 4) &
						 0x0f));
				ptk_tab[0] = ptk_table[note][0];
				ptk_tab[1] = ptk_table[note][1];
				ins = (c1 >> 2) & 0x1f;
				ptk_tab[0] |= (ins & 0xf0);
				ptk_tab[2] = (ins << 4) & 0xf0;
				fxt = c2 & 0x0f;
				ptk_tab[2] |= fxt;
				fxp = 0x00;
				ptk_tab[3] = fxp;
				fwrite (ptk_tab, 4, 1, out);
				continue;
			}
			if (c1 == 0xff) {
				bzero (ptk_tab, 5);
				fwrite (ptk_tab, 4, 1, out);
				continue;
			}
			fread (&c2, 1, 1, in);
			fread (&c3, 1, 1, in);
			note = (((c1 << 4) & 0x30) | ((c2 >> 4) & 0x0f));
			ptk_tab[0] = ptk_table[note][0];
			ptk_tab[1] = ptk_table[note][1];
			ins = (c1 >> 2) & 0x1f;
			ptk_tab[0] |= (ins & 0xf0);
			ptk_tab[2] = (ins << 4) & 0xf0;
			fxt = c2 & 0x0f;
			ptk_tab[2] |= fxt;
			fxp = c3;
			ptk_tab[3] = fxp;
			fwrite (ptk_tab, 4, 1, out);
		}
	}


	fseek (in, Add_Sample_Data, 0);

	t = (uint8 *) malloc (ssize);
	fread (t, ssize, 1, in);
	fwrite (t, ssize, 1, out);
	free (t);

	return 0;
}


static int test_DI (uint8 *data, int s)
{
	int ssize, start = 0;
	int j, k, l, m, n, o;

	PW_REQUEST_DATA (s, 21);

#if 0
	/* test #1 */
	if (i < 17) {
		Test = BAD;
		return;
	}
#endif

	/* test #2  (number of sample) */
	k = (data[start] << 8) + data[start + 1];
	if (k > 31)
		return -1;

	/* test #3 (finetunes and whole sample size) */
	/* k = number of samples */
	l = 0;
	for (j = 0; j < k; j++) {
		o = (((data[start + 14] << 8) + data[start + 15]) * 2);
		m = (((data[start + 18] << 8) + data[start + 19]) * 2);
		n = (((data[start + 20] << 8) + data[start + 21]) * 2);

		if ((o > 0xffff) || (m > 0xffff) || (n > 0xffff))
			return -1;

		if ((m + n) > o)
			return -1;

		if (data[start + 16 + j * 8] > 0x0f)
			return -1;

		if (data[start + 17 + j * 8] > 0x40)
			return -1;

		/* get total size of samples */
		l += o;
	}
	if (l <= 2)
		return -1;

	/* test #4 (addresses of pattern in file ... ptk_tableible ?) */
	/* k is still the number of sample */

	ssize = k;

	/* j is the address of pattern table now */
	j = (data[start + 2] << 24) + (data[start + 3] << 16)
		+ (data[start + 4] << 8) + data[start + 5];

	/* k is the address of the pattern data */
	k = (data[start + 6] << 24) + (data[start + 7] << 16)
		+ (data[start + 8] << 8) + data[start + 9];

	/* l is the address of the pattern data */
	l = (data[start + 10] << 24) + (data[start + 11] << 16)
		+ (data[start + 12] << 8) + data[start + 13];

	if (k <= j || l <= j || l <= k)
		return -1;

	if ((k - j) > 128)
		return -1;

#if 0
	if (k > in_size || l > in_size || l > in_size)
		return -1;
#endif

	/* test #4,1 :) */
	ssize *= 8;
	ssize += 2;
	if (j < ssize)
		return -1;

#if 0
	/* test #5 */
	if ((k + start) > in_size) {
		Test = BAD;
		return;
	}
#endif

	PW_REQUEST_DATA (s, start + k - 1);

	/* test pattern table reliability */
	for (m = j; m < (k - 1); m++) {
		if (data[start + m] > 0x80)
			return -1;
	}

	/* test #6  ($FF at the end of pattern list ?) */
	if (data[start + k - 1] != 0xFF)
		return -1;

	/* test #7 (addres of sample data > $FFFF ? ) */
	/* l is still the address of the sample data */
	if (l > 65535)
		return -1;

	return 0;
}
