/*
 * XANN_Packer.c   Copyright (C) 1997 Asle / ReDoX
 *                 Modified by Claudio Matsuoka
 *
 * XANN Packer to Protracker.
 *
 * NOTE: some lines will work ONLY on IBM-PC !!!. Check the lines
 *      with the warning note to get this code working on 68k machines.
 *
 ********************************************************
 *
 * 19990413  Sylvain Chipaux
 *  - no more open() of input file ... so no more fread() !.
 *    It speeds-up the process quite a bit :).
 *
 * 20000821  Claudio Matsuoka
 *  - heavy code cleanup
 */

/*
 * $Id: xann.c,v 1.2 2006-02-13 00:21:46 cmatsuoka Exp $
 */

#include <string.h>
#include "prowiz.h"

static int depack_XANN (uint8 *, FILE *);
static int test_XANN (uint8 *, int);

struct pw_format pw_xann = {
	"XANN",
	"XANN Packer",
	0x00,
	test_XANN,
	depack_XANN,
	NULL
};

#define SMP_DESC_ADDRESS 0x206
#define PAT_DATA_ADDRESS 0x43C

static int depack_XANN (uint8 *data, FILE *out)
{
	int start = 0;
	uint8 tmp[1025];
	uint8 c1, c2, c3, c4, c5, c6;
	uint8 ptable[128];
	uint8 Max = 0x00;
	uint8 note, ins, fxt, fxp;
	uint8 *address;
	uint8 fine, vol;
	uint8 Pattern[1025];
	long i = 0, j = 0, l = 0;
	long ssize = 0;
	long Where = start;

	bzero (tmp, 1025);
	bzero (ptable, 128);
	bzero (Pattern, 1025);

	/* title */
	fwrite (tmp, 20, 1, out);

	/* 31 samples */
	Where = start + SMP_DESC_ADDRESS;
	for (i = 0; i < 31; i++) {
		/* sample name */
		fwrite (tmp, 22, 1, out);

		/* read finetune */
		fine = data[Where++];

		/* read volume */
		vol = data[Where++];

		/* read loop start address */
		c1 = data[Where++];
		c2 = data[Where++];
		c3 = data[Where++];
		c4 = data[Where++];
		j = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

		/* read loop size */
		c5 = data[Where++];
		c6 = data[Where++];

		/* read sample address */
		c1 = data[Where++];
		c2 = data[Where++];
		c3 = data[Where++];
		c4 = data[Where++];
		l = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

		/* read & write sample size */
		c1 = data[Where++];
		c2 = data[Where++];
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		ssize += (((c1 << 8) + c2) * 2);

		/* calculate loop start value */
		j = j - l;

		/* write fine */
		fwrite (&fine, 1, 1, out);

		/* write vol */
		fwrite (&vol, 1, 1, out);

		/* write loop start */
		/* WARNING !!! WORKS ONLY ON PC !!!       */
		/* 68k machines code : c1 = *(address+2); */
		/* 68k machines code : c2 = *(address+3); */
		j /= 2;
		address = (uint8 *) & j;
		c1 = *(address + 1);
		c2 = *address;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		/* write loop size */
		fwrite (&c5, 1, 1, out);
		fwrite (&c6, 1, 1, out);

		/* bypass two unknown bytes */
		Where += 2;
	}

	/* pattern table */
	Max = 0x00;
	Where = start;
	for (c5 = 0; c5 < 128; c5++) {
		c1 = data[Where++];
		c2 = data[Where++];
		c3 = data[Where++];
		c4 = data[Where++];
		l =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		if (l == 0)
			break;
		ptable[c5] = ((l - 0x3c) / 1024) - 1;
		if (ptable[c5] > Max)
			Max = ptable[c5];
	}
	Max += 1;		/* starts at $00 */

	/* write number of pattern */
	fwrite (&c5, 1, 1, out);

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* write pattern list */
	for (i = 0; i < 128; i++)
		fwrite (&ptable[i], 1, 1, out);

	/* write Protracker's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data */
	Where = start + PAT_DATA_ADDRESS;
	for (i = 0; i < Max; i++) {
		for (j = 0; j < 256; j++) {
			ins = (data[Where + j * 4] >> 3) & 0x1f;
			note = data[Where + j * 4 + 1];
			fxt = data[Where + j * 4 + 2];
			fxp = data[Where + j * 4 + 3];
			switch (fxt) {
			case 0x00:	/* no fxt */
				fxt = 0x00;
				break;

			case 0x04:	/* arpeggio */
				fxt = 0x00;
				break;

			case 0x08:	/* portamento up */
				fxt = 0x01;
				break;

			case 0x0C:	/* portamento down */
				fxt = 0x02;
				break;

			case 0x10:	/* tone portamento with no fxp */
				fxt = 0x03;
				break;

			case 0x14:	/* tone portamento */
				fxt = 0x03;
				break;

			case 0x18:	/* vibrato with no fxp */
				fxt = 0x04;
				break;

			case 0x1C:	/* vibrato */
				fxt = 0x04;
				break;

			case 0x24:	/* tone portamento + vol slide DOWN */
				fxt = 0x05;
				break;

			case 0x28:	/* vibrato + volume slide UP */
				fxt = 0x06;
				c1 = (fxp << 4) & 0xf0;
				c2 = (fxp >> 4) & 0x0f;
				fxp = c1 | c2;
				break;

			case 0x2C:	/* vibrato + volume slide DOWN */
				fxt = 0x06;
				break;

			case 0x38:	/* sample offset */
				fxt = 0x09;
				break;

			case 0x3C:	/* volume slide up */
				fxt = 0x0A;
				c1 = (fxp << 4) & 0xf0;
				c2 = (fxp >> 4) & 0x0f;
				fxp = c1 | c2;
				break;

			case 0x40:	/* volume slide down */
				fxt = 0x0A;
				break;

			case 0x44:	/* position jump */
				fxt = 0x0B;
				break;

			case 0x48:	/* set volume */
				fxt = 0x0C;
				break;

			case 0x4C:	/* pattern break */
				fxt = 0x0D;
				break;

			case 0x50:	/* set speed */
				fxt = 0x0F;
				break;

			case 0x58:	/* set filter */
				fxt = 0x0E;
				fxp = 0x01;
				break;

			case 0x5C:	/* fine slide up */
				fxt = 0x0E;
				fxp |= 0x10;
				break;

			case 0x60:	/* fine slide down */
				fxt = 0x0E;
				fxp |= 0x20;
				break;

			case 0x84:	/* retriger */
				fxt = 0x0E;
				fxp |= 0x90;
				break;

			case 0x88:	/* fine volume slide up */
				fxt = 0x0E;
				fxp |= 0xa0;
				break;

			case 0x8C:	/* fine volume slide down */
				fxt = 0x0E;
				fxp |= 0xb0;
				break;

			case 0x94:	/* note delay */
				fxt = 0x0E;
				fxp |= 0xd0;
				break;

			case 0x98:	/* pattern delay */
				fxt = 0x0E;
				fxp |= 0xe0;
				break;

			default:
				printf ("%x : at %ld (out:%ld)\n", fxt,
					Where + (j * 4), ftell (out));
				fxt = 0x00;
				break;
			}
			Pattern[j * 4] = (ins & 0xf0);
			Pattern[j * 4] |= ptk_table[(note / 2)][0];
			Pattern[j * 4 + 1] = ptk_table[(note / 2)][1];
			Pattern[j * 4 + 2] = ((ins << 4) & 0xf0);
			Pattern[j * 4 + 2] |= fxt;
			Pattern[j * 4 + 3] = fxp;
		}
		Where += 1024;
		fwrite (Pattern, 1024, 1, out);
	}

	/* sample data */
	fwrite (&data[Where], ssize, 1, out);

	return 0;
}


static int test_XANN (uint8 *data, int s)
{
	int i = 0, j, k, l, m;
	int start = 0;

	PW_REQUEST_DATA (s, 2048);

	/* test 1 */
	if (data[i + 3] != 0x3c)
		return -1;

	/* test 2 */
	for (l = 0; l < 128; l++) {
		j = (data[start + l * 4] << 24)
			+ (data[start + l * 4 + 1] << 16)
			+ (data[start + l * 4 + 2] << 8)
			+ data[start + l * 4 + 3];
		k = (j / 4) * 4;
		if (k != j || j > 132156)
			return -1;
	}

#if 0
	/* test 3 */
	if ((size - start) < 2108)
		return -1;
#endif

	/* test 4 */
	for (j = 0; j < 64; j++) {
		if (data[start + 3 + j * 4] != 0x3c &&
			data[start + 3 + j * 4] != 0x00) {
			return -1;
		}
	}

	/* test 5 */
	for (j = 0; j < 31; j++) {
		if (data[start + 519 + 16 * j] > 0x40)
			return -1;
	}

	/* test #6  (address of samples) */
	for (l = 0; l < 30; l++) {
		k = (data[start + 526 + 16 * l] << 24) +
			(data[start + 527 + 16 * l] << 16) +
			(data[start + 528 + 16 * l] << 8) +
			data[start + 529 + 16 * l];

		j = (((data[start + 524 + 16 * l] << 8) +
			data[start + 525 + 16 * l]) * 2);

		m = (data[start + 520 + 16 * (l + 1)] << 24) +
			(data[start + 521 + 16 * (l + 1)] << 16) +
			(data[start + 522 + 16 * (l + 1)] << 8) +
			data[start + 523 + 16 * (l + 1)];

		if (k < 2108 || m < 2108)
			return -1;

		if (k > m)
			return -1;
	}

#if 0
	/* test #7  first pattern data .. */
	for (j = 0; j < 256; j++) {
#if 0
		k = data[start + j * 4 + 1085] / 2;
		l = k * 2;
		if (data[start + j * 4 + 1085] != l)
			return -1;
#endif
		if (data[start + j * 4 + 1085] & 1)
			return -1;
	}
#endif

	return 0;
}
