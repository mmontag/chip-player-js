/*
 * ac1d.c   Copyright (C) 1996-1997 Asle / ReDoX
 *	    Modified by Claudio Matsuoka
 *
 * Converts AC1D packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * $Id: ac1d.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <stdlib.h>
#include <string.h>
#include "prowiz.h"

static int depack_AC1D (FILE *, FILE *);
static int test_AC1D (uint8 *, int);

struct pw_format pw_ac1d = {
	"AC1D",
	"AC1D Packer",
	0x00,
	test_AC1D,
	NULL,
	depack_AC1D
};


static int depack_AC1D (FILE *in, FILE *out)
{
	uint8 NO_NOTE = 0xff;
	uint8 c1, c2, c3, c4;
	uint8 npos;
	uint8 ntk_byte;
	uint8 *tmp;
	uint8 Nbr_Pat;
	uint8 note, ins, fxt, fxp;
	long saddr;
	long ssize = 0;
	long paddr[128];
	long psize[128];
	long tsize1, tsize2, tsize3;
	long i, j, k;

	bzero (paddr, 128 * 4);
	bzero (psize, 128 * 4);

	fread (&npos, 1, 1, in);
	fread (&ntk_byte, 1, 1, in);

	/* bypass ID */
	fseek (in, 2, 1);	/* SEEK_CUR */

	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	saddr = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
	/*printf ( "adress of sample datas : %ld\n" , saddr ); */

	/* write title */
	tmp = (uint8 *) malloc (20);
	bzero (tmp, 20);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	tmp = (uint8 *) malloc (22);
	bzero (tmp, 22);
	for (i = 0; i < 31; i++) {
		fwrite (tmp, 22, 1, out);

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

	/* pattern addresses */
	for (Nbr_Pat = 0; Nbr_Pat < 128; Nbr_Pat++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		paddr[Nbr_Pat] = (c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		if (paddr[Nbr_Pat] == 0)
			break;
	}
	Nbr_Pat -= 1;
	/*printf ( "Number of pattern saved : %d\n" , Nbr_Pat ); */

	for (i = 0; i < (Nbr_Pat - 1); i++) {
		psize[i] =
			paddr[i + 1] - paddr[i]; }


	/* write number of pattern pos */
	fwrite (&npos, 1, 1, out);

	/* write "noisetracker" byte */
	fwrite (&ntk_byte, 1, 1, out);

	/* go to pattern table .. */
	fseek (in, 0x300, 0);	/* SEEK_SET */

	/* pattern table */
	for (i = 0; i < 128; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
	}

	/* write ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < Nbr_Pat; i++) {
		fseek (in, paddr[i], 0);	/* SEEK_SET */
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		tsize1 = (c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		tsize2 = (c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		tsize3 = (c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		bzero (tmp, 1024);
		for (k = 0; k < 4; k++) {
			for (j = 0; j < 64; j++) {
				note = ins = fxt = fxp = 0x00;
				fread (&c1, 1, 1, in);
				if ((c1 & 0x80) == 0x80) {
					c4 = c1 & 0x7f;
					j += (c4 - 1);
					continue;
				}
				fread (&c2, 1, 1, in);
				ins = ((c1 & 0xc0) >> 2);
				ins |= ((c2 >> 4) & 0x0f);
				note = c1 & 0x3f;
				if (note == 0x3f)
					note = NO_NOTE;
				else if (note != 0x00)
					note -= 0x0b;
				if (note == 0x00)
					note += 0x01;
				tmp[j * 16 + k * 4] = ins & 0xf0;
				if (note != NO_NOTE) {
					tmp[j * 16 + k * 4] |=
						ptk_table[note][0];
					tmp[j * 16 + k * 4 + 1] =
						ptk_table[note][1];
				}
				if ((c2 & 0x0f) == 0x07) {
					fxt = 0x00;
					fxp = 0x00;
					tmp[j * 16 + k * 4 + 2] =
						(ins << 4) & 0xf0;
					continue;
				}
				fread (&c3, 1, 1, in);
				fxt = c2 & 0x0f;
				fxp = c3;
				tmp[j * 16 + k * 4 + 2] =
					((ins << 4) & 0xf0);
				tmp[j * 16 + k * 4 + 2] |= fxt;
				tmp[j * 16 + k * 4 + 3] = fxp;
			}
		}
		fwrite (tmp, 1024, 1, out);
		/*printf ( "+" ); */
	}
	free (tmp);
	/*printf ( "\n" ); */

	/* sample data */
	fseek (in, saddr, 0);
	tmp = (uint8 *) malloc (ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}


static int test_AC1D (uint8 *data, int s)
{
	int j, k;
	int start = 0;

	PW_REQUEST_DATA (s, 896);

	/* test #1 */
	if (data[2] != 0xac || data[3] != 0x1d)
		return -1;

	/* test #2 */
	if (data[start] > 0x7f)
		return -1;

	/* test #4 */
	for (k = 0; k < 31; k++) {
		if (data[start + 10 + 8 * k] > 0x0f)
			return -1;
	}

	/* test #5 */
	for (j = 0; j < 128; j++) {
		if (data[start + 768 + j] > 0x7f)
			return -1;
	}

	return 0;
}
