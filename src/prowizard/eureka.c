/*
 * EurekaPacker.c   Copyright (C) 1997 Asle / ReDoX
 *		    Modified by Claudio Matsuoka
 *
 * Converts MODs packed with Eureka packer back to ptk
 *
 * $Id: eureka.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_eu (uint8 *, int);
static int depack_eu (FILE *, FILE *);

struct pw_format pw_eu = {
	"EU",
	"Eureka Packer",
	0x00,
	test_eu,
	NULL,
	depack_eu
};

static int depack_eu (FILE * in, FILE * out)
{
	uint8 *tmp;
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 npat = 0x00;
	uint8 ptable[128];
	uint8 pat_max = 0x00;
	long Sample_Start_Address = 0;
	long ssize = 0;
	long Track_Address[128][4];
	long i = 0, j = 0, k;

	bzero (ptable, 128);

	/* read header ... same as ptk */
	tmp = (uint8 *) malloc (1080);
	bzero (tmp, 1080);
	fread (tmp, 1080, 1, in);
	fwrite (tmp, 1080, 1, out);

	/* now, let's sort out that a bit :) */
	/* first, the whole sample size */
	for (i = 0; i < 31; i++)
		ssize +=
			(((tmp[i * 30 + 42] << 8) + tmp[i * 30 +
					43]) * 2);
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* next, the size of the pattern list */
	npat = tmp[950];
	/*printf ( "Size of pattern list : %d\n" , npat ); */

	/* now, the pattern list .. and the max */
	pat_max = 0x00;
	for (i = 0; i < 128; i++) {
		ptable[i] = tmp[952 + i];
		if (ptable[i] > pat_max)
			pat_max = ptable[i];
	}
	pat_max += 1;
	/*printf ( "Number of patterns : %d\n" , pat_max ); */
	free (tmp);

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);


	/* read sample data address */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Sample_Start_Address =
		(c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
	/*printf ( "Address of sample data : %ld\n" , Sample_Start_Address ); */

	/* read tracks addresses */
	for (i = 0; i < pat_max; i++) {
		for (j = 0; j < 4; j++) {
			fread (&c1, 1, 1, in);
			fread (&c2, 1, 1, in);
			Track_Address[i][j] = (c1 << 8) + c2;
		}
	}

	/* the track data now ... */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < pat_max; i++) {
		bzero (tmp, 1024);
		for (j = 0; j < 4; j++) {
			fseek (in, Track_Address[i][j], 0);	/* SEEK_SET */
			for (k = 0; k < 64; k++) {
				fread (&c1, 1, 1, in);
				if ((c1 & 0xc0) == 0x00) {
					fread (&c2, 1, 1, in);
					fread (&c3, 1, 1, in);
					fread (&c4, 1, 1, in);
					tmp[k * 16 + j * 4] = c1;
					tmp[k * 16 + j * 4 + 1] = c2;
					tmp[k * 16 + j * 4 + 2] = c3;
					tmp[k * 16 + j * 4 + 3] = c4;
					continue;
				}
				if ((c1 & 0xc0) == 0xc0) {
					k += (c1 & 0x3f);
					continue;
				}
				if ((c1 & 0xc0) == 0x40) {
					fread (&c2, 1, 1, in);
					tmp[k * 16 + j * 4 + 2] =
						c1 & 0x0f;
					tmp[k * 16 + j * 4 + 3] = c2;
					continue;
				}
				if ((c1 & 0xc0) == 0x80) {
					fread (&c2, 1, 1, in);
					fread (&c3, 1, 1, in);
					tmp[k * 16 + j * 4] = c2;
					tmp[k * 16 + j * 4 + 1] = c3;
					tmp[k * 16 + j * 4 + 2] =
						(c1 << 4) & 0xf0;
					continue;
				}
			}
		}
		fwrite (tmp, 1024, 1, out);
		/*printf ( "+" ); */
	}
	free (tmp);

	/* go to sample data addy */
	fseek (in, Sample_Start_Address, 0);	/* SEEK_SET */

	/* read sample data */
	tmp = (uint8 *) malloc (ssize);
	bzero (tmp, ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}

static int test_eu (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0;

	PW_REQUEST_DATA (s, 1084);

	/* test 2 */
	j = data[start + 950];
	if (j == 0 || j > 127)
		return -1;

	/* test #3  finetunes & volumes */
	for (k = 0; k < 31; k++) {
		o = (data[start + 42 + k * 30] << 8) +
			data[start + 43 + k * 30];
		m = (data[start + 46 + k * 30] << 8) +
			data[start + 47 + k * 30];
		n = (data[start + 48 + k * 30] << 8) +
			data[start + 49 + k * 30];
		o *= 2;
		m *= 2;
		n *= 2;
		if (o > 0xffff || m > 0xffff || n > 0xffff)
			return -1;

		if ((m + n) > (o + 2))
			return -1;

		if (data[start + 44 + k * 30] > 0x0f ||
			data[start + 45 + k * 30] > 0x40)
			return -1;
	}


	/* test 4 */
	l = (data[start + 1080] << 24) + (data[start + 1081] << 16)
		+ (data[start + 1082] << 8) + data[start + 1083];

#if 0
	if ((l + start) > in_size)
		return -1;
#endif

	if (l < 1084)
		return -1;

	m = 0;
	/* pattern list */
	for (k = 0; k < j; k++) {
		n = data[start + 952 + k];
		if (n > m)
			m = n;
		if (n > 127)
			return -1;
	}
	k += 2;		/* to be sure .. */

	while (k != 128) {
		if (data[start + 952 + k] != 0)
			return -1;
		k += 1;
	}
	m += 1;
	/* m is the highest pattern number */


	/* test #5 */
	/* j is still the size if the pattern table */
	/* l is still the address of the sample data */
	/* m is the highest pattern number */
	n = 0;
	j = 999999L;

	PW_REQUEST_DATA (s, start + (m * 4) * 2 + 1085);

	for (k = 0; k < (m * 4); k++) {
		o = (data[start + k * 2 + 1084] << 8) +
			data[start + k * 2 + 1085];
		if (o > l || o < 1084)
			return -1;
		if (o > n)
			n = o;
		if (o < j)
			j = o;
	}
	/* o is the highest track address */
	/* j is the lowest track address */

	/* test track datas */
	/* last track wont be tested ... */
	for (k = j; k < o; k++) {
		if ((data[start + k] & 0xC0) == 0xC0)
			continue;
		if ((data[start + k] & 0xC0) == 0x80) {
			k += 2;
			continue;
		}
		if ((data[start + k] & 0xC0) == 0x40) {
			if ((data[start + k] & 0x3F) == 0x00 &&
				data[start + k + 1] == 0x00)
				return -1;
			k += 1;
			continue;
		}
		if ((data[start + k] & 0xC0) == 0x00) {
			if (data[start + k] > 0x13)
				return -1;
			k += 3;
			continue;
		}
	}

	return 0;
}
