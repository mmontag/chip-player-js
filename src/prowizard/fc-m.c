/*
 * FC-M_Packer.c   Copyright (C) 1997 Asle / ReDoX
 *                 Copyright (c) 2006-2007 Claudio Matsuoka
 *
 * Converts back to ptk FC-M packed MODs
 *
 * $Id: fc-m.c,v 1.3 2007-09-18 02:15:05 cmatsuoka Exp $
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


static int depack_fcm(FILE *in, FILE *out)
{
	uint8 c1;
	uint8 ptable[128];
	uint8 pat_pos;
	uint8 pat_max = 0x00;
	uint8 *tmp;
	uint8 pat_data[1024];
	int i = 0, j = 0;
	int size, ssize = 0;

	bzero (ptable, 128);

	read32b(in);	/* bypass "FC-M" ID */
	read16b(in);	/* bypass what looks like the version number .. */
	read32b(in);	/* bypass "NAME" chunk */

	/* read and write title */
	for (i = 0; i < 20; i++)
		write8(out, read8(in));

	read32b(in);	/* bypass "INST" chunk */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {
		for (j = 0; j < 22; j++)	/*sample name */
			write8(out, 0);

		write16b(out, size = read16b(in));	/* size */
		ssize += size * 2;
		write8(out, read8(in));		/* finetune */
		write8(out, read8(in));		/* volume */
		write16b(out, read16b(in));	/* loop start */
		size = read16b(in);		/* loop size */
		if (size == 0)
			size = 1;
		write16b(out, size);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	read32b(in);	/* bypass "LONG" chunk */

	/* read and write pattern table lenght */
	write8(out, pat_pos = read8(in));
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* read and write NoiseTracker byte */
	write8(out, read8(in));

	read32b(in);	/* bypass "PATT" chunk */

	/* read and write pattern list and get highest patt number */
	for (i = 0; i < pat_pos; i++) {
		write8(out, c1 = read8(in));
		if (c1 > pat_max)
			pat_max = c1;
	}
	for (; i < 128; i++)
		write8(out, 0);
	/*printf ( "Number of pattern : %d\n" , pat_max + 1 ); */

	/* write ptk's ID */
	write32b(out, 0x4E2E4B2E);

	read32b(in);	/* bypass "SONG" chunk */

	/* pattern data */
	for (i = 0; i <= pat_max; i++) {
		fread(pat_data, 1024, 1, in);
		fwrite(pat_data, 1024, 1, out);
		/*printf ( "+" ); */
	}
	/*printf ( "\n" ); */


	read32b(in);	/* bypass "SAMP" chunk */

	/* sample data */
	tmp = (uint8 *)malloc(ssize);
	fread(tmp, ssize, 1, in);
	fwrite(tmp, ssize, 1, out);
	free(tmp);

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
