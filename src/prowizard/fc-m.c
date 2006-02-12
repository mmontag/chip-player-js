/*
 * FC-M_Packer.c   Copyright (C) 1997 Asle / ReDoX
 *                 Modified by Claudio Matsuoka
 *
 * Converts back to ptk FC-M packed MODs
 *
 * $Id: fc-m.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_fcm (uint8 *, int);
static int depack_fcm (FILE *, FILE *);

struct pw_format pw_fcm = {
	"FCM",
	"FC-M Packer",
	0x00,
	test_fcm,
	NULL,
	depack_fcm
};


static int depack_fcm (FILE * in, FILE * out)
{
	uint8 c1, c2, c3;
	uint8 ptable[128];
	uint8 pat_pos;
	uint8 pat_max = 0x00;
	uint8 *tmp;
	uint8 Pattern[1024];
	long i = 0, j = 0;
	long ssize = 0;

	bzero (ptable, 128);

	/* bypass "FC-M" ID */
	fseek (in, 4, 0);

	/* bypass what looks like the version number .. */
	fseek (in, 2, 1);

	/* bypass "NAME" chunk */
	fseek (in, 4, 1);

	/* read and write title */
	for (i = 0; i < 20; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
	}

	/* bypass "INST" chunk */
	fseek (in, 4, 1);

	/* read and write sample descriptions */
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
		if ((c1 == 0x00) && (c2 == 0x00))
			c2 = 0x01;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* bypass "LONG" chunk */
	fseek (in, 4, 1);	/* SEEK_CUR */

	/* read and write pattern table lenght */
	fread (&pat_pos, 1, 1, in);
	fwrite (&pat_pos, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* read and write NoiseTracker byte */
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);

	/* bypass "PATT" chunk */
	fseek (in, 4, 1);

	/* read and write pattern list and get highest patt number */
	for (i = 0; i < pat_pos; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		if (c1 > pat_max)
			pat_max = c1;
	}
	c2 = 0x00;
	while (i < 128) {
		fwrite (&c2, 1, 1, out);
		i += 1;
	}
	/*printf ( "Number of pattern : %d\n" , pat_max + 1 ); */

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* bypass "SONG" chunk */
	fseek (in, 4, 1);

	/* pattern data */
	for (i = 0; i <= pat_max; i++) {
		bzero (Pattern, 1024);
		fread (Pattern, 1024, 1, in);
		fwrite (Pattern, 1024, 1, out);
		/*printf ( "+" ); */
	}
	/*printf ( "\n" ); */


	/* bypass "SAMP" chunk */
	fseek (in, 4, 1);

	/* sample data */
	tmp = (uint8 *) malloc (ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}


static int test_fcm (uint8 *data, int s)
{
	int start = 0;
	int j;

	PW_REQUEST_DATA (s, 37 + 8 * 31);

	/* "FC-M" : ID of FC-M packer */
	if (data[0] != 'F' || data[1] != 'C' || data[2] != '-' ||
		data[3] != 'M')
		return -1;

	/* test 1 */
	if (data[start + 4] != 0x01)
		return -1;

	/* test 2 */
	if (data[start + 5] != 0x00)
		return -1;

	/* test 3 */
	for (j = 0; j < 31; j++) {
		if (data[start + 37 + 8 * j] > 0x40)
			return -1;
	}

	return 0;
}
