/*
 * Heatseeker_mc1.0.c   Copyright (C) 1997 Asle / ReDoX
 *			Modified by Claudio Matsuoka
 *
 * Converts back to ptk Heatseeker packed MODs
 *
 * note: There's a good job ! .. gosh !.
 *
 * $Id: heatseek.c,v 1.1 2006-02-12 22:04:42 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_crb (uint8 *, int);
static int depack_crb (FILE *, FILE *);

struct pw_format pw_crb = {
	"CRB",
	"Heatseeker 1.0",
	0x00,
	test_crb,
	NULL,
	depack_crb
};


static int depack_crb (FILE * in, FILE * out)
{
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 ptable[128];
	uint8 pat_pos;
	uint8 pat_max = 0x00;
	uint8 *tmp;
	uint8 pat[1024];
	long taddr[512];
	long i = 0, j = 0, k = 0, l = 0, m;
	long ssize = 0;

	bzero (ptable, 128);
	bzero (taddr, 512 * 4);

	/* write title */
	for (i = 0; i < 20; i++)
		fwrite (&c1, 1, 1, out);

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

	/* read and write pattern table lenght */
	fread (&pat_pos, 1, 1, in);
	fwrite (&pat_pos, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* read and write NoiseTracker byte */
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);

	/* read and write pattern list and get highest patt number */
	for (i = 0; i < 128; i++) {
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
		if (c1 > pat_max)
			pat_max = c1;
	}
	pat_max += 1;
	/*printf ( "Number of pattern : %d\n" , pat_max ); */

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data */
	for (i = 0; i < pat_max; i++) {
/*fprintf ( info , "\n\n\npat %ld :\n" , i );*/
		bzero (pat, 1024);
		for (j = 0; j < 4; j++) {
			taddr[i * 4 + j] = ftell (in);
/*fprintf ( info , "Voice %ld (at:%ld):\n" , j , taddr[i*4+j]);*/
			for (k = 0; k < 64; k++) {
				fread (&c1, 1, 1, in);
/*fprintf ( info , "%2ld: %2x , " , k , c1 );*/
				if (c1 == 0x80) {
					fread (&c2, 1, 1, in);
					fread (&c3, 1, 1, in);
					fread (&c4, 1, 1, in);
/*fprintf ( info , "%2x , %2x , %2x !!! (%ld)\n" , c2 , c3 , c4 ,ftell (in ));*/
					k += c4;
					continue;
				}
				if (c1 == 0xc0) {
					fread (&c2, 1, 1, in);
					fread (&c3, 1, 1, in);
					fread (&c4, 1, 1, in);
					l = ftell (in);
					fseek (in,
						taddr[((c3 << 8) +
								c4) / 4], 0);
/*fprintf ( info , "now at %ld (voice : %d)\n" , ftell ( in ) , ((c3*256)+c4)/4 );*/
					for (m = 0; m < 64; m++) {
						fread (&c1, 1, 1, in);
/*fprintf ( info , "%2ld: %2x , " , k , c1 );*/
						if (c1 == 0x80) {
							fread (&c2, 1, 1, in);
							fread (&c3, 1, 1, in);
							fread (&c4, 1, 1, in);
/*fprintf ( info , "%2x , %2x , %2x !!! (%ld)\n" , c2 , c3 , c4 ,ftell (in ));*/
							m += c4;
							continue;
						}
						fread (&c2, 1, 1, in);
						fread (&c3, 1, 1, in);
						fread (&c4, 1, 1, in);
/*fprintf ( info , "%2x , %2x , %2x  (%ld)\n" , c2 , c3 , c4 ,ftell (in));*/
						pat[m * 16 + j * 4] = c1;
						pat[m * 16 + j * 4 + 1] =
							c2;
						pat[m * 16 + j * 4 + 2] =
							c3;
						pat[m * 16 + j * 4 + 3] =
							c4;
					}
/*fprintf ( info , "%2x , %2x , %2x ??? (%ld)\n" , c2 , c3 , c4 ,ftell (in ));*/
					fseek (in, l, 0);	/* SEEK_SET */
					k += 100;
					continue;
				}
				fread (&c2, 1, 1, in);
				fread (&c3, 1, 1, in);
				fread (&c4, 1, 1, in);
/*fprintf ( info , "%2x , %2x , %2x  (%ld)\n" , c2 , c3 , c4 ,ftell (in));*/
				pat[k * 16 + j * 4] = c1;
				pat[k * 16 + j * 4 + 1] = c2;
				pat[k * 16 + j * 4 + 2] = c3;
				pat[k * 16 + j * 4 + 3] = c4;
			}
		}
		fwrite (pat, 1024, 1, out);
		/*printf ( "+" ); */
		/*fflush ( stdout ); */
	}
	/*printf ( "\n" ); */

	/* sample data */
/*printf ( "where : %ld  (wholesamplesize : %ld)\n" , ftell ( in ) , ssize );*/
	tmp = (uint8 *) malloc (ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);

	return 0;
}


static int test_crb (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	PW_REQUEST_DATA (s, 378);

	/* size of the pattern table */
	if (data[start + 248] > 0x7f || data[start + 248] == 0x00)
		return -1;

	/* test noisetracker byte */
	if (data[start + 249] != 0x7f)
		return -1;

	/* test samples */
	ssize = 0;
	for (k = 0; k < 31; k++) {
		if (data[start + 2 + k * 8] > 0x0f)
			return -1;

		/* test volumes */
		if (data[start + 3 + k * 8] > 0x40)
			return -1;

		/* size */
		j = (data[start + k * 8] << 8) + data[start + 1 + k * 8];
		/* loop start */
		m = (data[start + k * 8 + 4] << 8) + data[start + 5 + k * 8];
		/* loop size */
		n = (data[start + k * 8 + 6] << 8) + data[start + 7 + k * 8];
		j *= 2;
		m *= 2;
		n *= 2;

		if (j > 0xFFFF || m > 0xFFFF || n > 0xFFFF)
			return -1;

		/* n != 2 test added by claudio -- asle, please check! */
		if (n != 0 && n != 2 && (m + n) > j)
			return -1;

		if (m != 0 && n <= 2)
			return -1;

		ssize += j;
	}

printf ("3\n");
	if (ssize <= 4)
		return -1;

	/* test pattern table */
	l = 0;
	for (j = 0; j < 128; j++) {
		if (data[start + 250 + j] > 0x7f)
			return -1;
		if (data[start + 250 + j] > l)
			l = data[start + 250 + j];
	}

	/* FIXME */
	PW_REQUEST_DATA (s, 379 + 4 * l * 4 * 64);

	/* test notes */
	k = 0;
	j = 0;
	for (m = 0; m <= l; m++) {
		for (n = 0; n < 4; n++) {
			for (o = 0; o < 64; o++) {
				switch (data[start + 378 + j] & 0xC0) {
				case 0x00:
					if ((data[start + 378 + j] & 0x0F) > 0x03)
						return -1;
					k += 4;
					j += 4;
					break;
				case 0x80:
					if (data[start + 379 + j] != 0x00)
						return -1;
					o += data[start + 381 + j];
					j += 4;
					k += 4;
					break;
				case 0xC0:
					if (data[start + 379 + j] != 0x00)
						return -1;
					o = 100;
					j += 4;
					k += 4;
					break;
				default:
					break;
				}
			}
		}
	}

	/* k is the size of the pattern data */
	/* ssize is the size of the sample data */

	return 0;
}
