/*
 * ProRunner1.c   Copyright (C) 1996 Asle / ReDoX
 *                Modified by Claudio Matsuoka
 *
 * Converts MODs converted with Prorunner packer v1.0
 *
 * $Id: prun1.c,v 1.1 2006-02-12 22:04:43 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_pru1 (uint8 *, int);
static int depack_pru1 (FILE *, FILE *);

struct pw_format pw_pru1 = {
	"PRU1",
	"Prorunner 1.0",
	0x00,
	test_pru1,
	NULL,
	depack_pru1
};

static int depack_pru1 (FILE * in, FILE * out)
{
	uint8 Header[2048];
	uint8 *tmp;
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 npat = 0x00;
	uint8 ptable[128];
	uint8 Max = 0x00;
	long ssize = 0;
	long i = 0, j = 0;

	bzero (Header, 2048);
	bzero (ptable, 128);

	/* read and write whole header */
	fseek (in, 0, SEEK_SET);
	fread (Header, 950, 1, in);
	fwrite (Header, 950, 1, out);

	/* get whole sample size */
	for (i = 0; i < 31; i++) {
		ssize +=
			(((Header[42 + i * 30] << 8) + Header[43 +
			     i * 30]) * 2);
	}
	/*printf ( "Whole sanple size : %ld\n" , ssize ); */

	/* read and write size of pattern list */
	fread (&npat, 1, 1, in);
	fwrite (&npat, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , npat ); */

	bzero (Header, 2048);

	/* read and write ntk byte and pattern list */
	fread (Header, 129, 1, in);
	fwrite (Header, 129, 1, out);

	/* write ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* get number of pattern */
	Max = 0x00;
	for (i = 1; i < 129; i++) {
		if (Header[i] > Max)
			Max = Header[i];
	}
	/*printf ( "Number of pattern : %d\n" , Max ); */

	/* pattern data */
	fseek (in, 1084, SEEK_SET);
	for (i = 0; i <= Max; i++) {
		for (j = 0; j < 256; j++) {
			fread (&Header[0], 1, 1, in);
			fread (&Header[1], 1, 1, in);
			fread (&Header[2], 1, 1, in);
			fread (&Header[3], 1, 1, in);
			c1 = Header[0] & 0xf0;
			c3 = (Header[0] & 0x0f) << 4;
			c3 |= Header[2];
			c4 = Header[3];
			c1 |= ptk_table[Header[1]][0];
			c2 = ptk_table[Header[1]][1];
			fwrite (&c1, 1, 1, out);
			fwrite (&c2, 1, 1, out);
			fwrite (&c3, 1, 1, out);
			fwrite (&c4, 1, 1, out);
		}
	}


	/* sample data */
	tmp = (uint8 *) malloc (ssize);
	bzero (tmp, ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}


static int test_pru1 (uint8 *data, int s)
{
	int start = 0;

	PW_REQUEST_DATA (s, 1080);

	if (data[1080] != 'S' || data[1081] != 'N' ||
		data[1082] != 'T' || data[1083] != '.')
		return -1;

	/* test 2 */
	if (data[start + 951] != 0x7f)
		return -1;

	/* test 3 */
	if (data[start + 950] > 0x7f)
		return -1;

	return 0;
}

