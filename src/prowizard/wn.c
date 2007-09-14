/*
 * Wanton_Packer.c   Copyright (C) 1997 Asle / ReDoX
 *                   Modified by Claudio Matsuoka
 *
 * Converts MODs converted with Wanton packer
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 */

/*
 * $Id: wn.c,v 1.2 2007-09-14 18:40:58 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_wn (uint8 *, int);
static int depack_wn (uint8 *, FILE *);

struct pw_format pw_wn = {
	"WN",
	"Wanton Packer",
	0x00,
	test_wn,
	depack_wn,
	NULL
};


static int depack_wn (uint8 *data, FILE * out)
{
	uint8 c1, c2, c3, c4;
	uint8 npat = 0x00;
	uint8 Max = 0x00;
	int ssize = 0;
	int i = 0, j = 0;
	int start = 0;
	int w = start;		/* main pointer to prevent fread() */

	/* read header */
	fwrite (&data[w], 950, 1, out);

	/* get whole sample size */
	for (i = 0; i < 31; i++) {
		ssize += (((data[w + 42 + i * 30] << 8) +
		     data[w + 43 + i * 30]) << 1);
	}

	/* read size of pattern list */
	w = start + 950;
	npat = data[w++];
	fwrite (&npat, 1, 1, out);

	i = w;
	fwrite (&data[w], 129, 1, out);
	w += 129;

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* get highest pattern number */
	Max = 0x00;
	for (i = 0; i < 128; i++) {
		if (data[start + 952 + i] > Max)
			Max = data[start + 952 + i];
	}
	Max += 1;

	/* pattern data */
	w = start + 1084;
	for (i = 0; i < Max; i++) {
		for (j = 0; j < 256; j++) {
			c1 = data[w + 1] & 0xf0;
			c1 |= ptk_table[(data[w] / 2)][0];
			c2 = ptk_table[(data[w] / 2)][1];
			c3 = (data[w + 1] << 4) & 0xf0;
			c3 |= data[w + 2];
			c4 = data[w + 3];

			fwrite (&c1, 1, 1, out);
			fwrite (&c2, 1, 1, out);
			fwrite (&c3, 1, 1, out);
			fwrite (&c4, 1, 1, out);
			w += 4;
		}
	}

	/* sample data */
	fwrite (&data[w], ssize, 1, out);

	return 0;
}

static int test_wn (uint8 *data, int s)
{
	int start = 0;

	PW_REQUEST_DATA (s, 1082);

	/* test 1 */
	if (data[1080] != 'W' || data[1081] !='N')
		return -1;

	/* test 2 */
	if (data[start + 951] != 0x7f)
		return -1;

	/* test 3 */
	if (data[start + 950] > 0x7f)
		return -1;

	return 0;
}
