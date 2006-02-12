/*
 *   Hornet_Packer.c   1997 (c) Asle / ReDoX
 *
 * Converts MODs converted with Hornet packer
 * GCC Hornet_Packer.c -o Hornet_Packer -Wall -O3
 *
*/

#include <string.h>
#include <stdlib.h>

void Depack_HRT (FILE * in, FILE * out)
{
	uint8 Header[2048];
	uint8 *tmp;
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 npat = 0x00;
	uint8 ptable[128];
	uint8 ptk_table[37][2];
	uint8 Max = 0x00;
	long ssize = 0;
	long i = 0, j = 0;
	// FILE *in,*out;

	if (Save_Status == BAD)
		return;

#include "ptktable.h"

	// in = fdopen (fd_in, "rb");
	// sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
	// out = fdopen (fd_out, "w+b");

	bzero (Header, 2048);
	bzero (ptable, 128);

	/* read header */
	fread (Header, 950, 1, in);

	/* empty-ing those adresse values ... */
	for (i = 0; i < 31; i++) {
		Header[38 + (30 * i)] = 0x00;
		Header[38 + (30 * i) + 1] = 0x00;
		Header[38 + (30 * i) + 2] = 0x00;
		Header[38 + (30 * i) + 3] = 0x00;
	}

	/* write header */
	fwrite (Header, 950, 1, out);

	/* get whole sample size */
	for (i = 0; i < 31; i++) {
		ssize +=
			(((Header[42 + (30 * i)] << 8) + Header[43 +
					30 * i]) * 2);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* read number of pattern */
	fread (&npat, 1, 1, in);
	fwrite (&npat, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , npat ); */

	/* read noisetracker byte and pattern list */
	bzero (Header, 2048);
	fread (Header, 129, 1, in);
	fwrite (Header, 129, 1, out);

	/* get number of pattern */
	Max = 0x00;
	for (i = 1; i < 129; i++) {
		if (Header[i] > Max)
			Max = Header[i];
	}
	/*printf ( "Number of pattern : %d\n" , Max ); */

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* pattern data */
	fseek (in, 1084, SEEK_SET);
	for (i = 0; i <= Max; i++) {
		for (j = 0; j < 256; j++) {
			fread (&Header[0], 1, 1, in);
			fread (&Header[1], 1, 1, in);
			fread (&Header[2], 1, 1, in);
			fread (&Header[3], 1, 1, in);
			Header[0] /= 2;
			c1 = Header[0] & 0xf0;
			if (Header[1] == 0x00)
				c2 = 0x00;
			else {
				c1 |= ptk_table[(Header[1] / 2)][0];
				c2 = ptk_table[(Header[1] / 2)][1];
			}
			c3 = (Header[0] << 4) & 0xf0;
			c3 |= Header[2];
			c4 = Header[3];

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

	/* crap */
	Crap ("HRT:Hornet Packer", BAD, BAD, out);

	fflush (in);
	fflush (out);

	printf ("done\n");
	return;			/* useless ... but */
}

#include <string.h>
#include <stdlib.h>

void testHRT (void)
{
	/* test 1 */
	if (i < 1080) {
		Test = BAD;
		return;
	}

	/* test 2 */
	start = i - 1080;
	for (j = 0; j < 31; j++) {
		if (data[45 + j * 30 + start] > 0x40) {
			Test = BAD;
			return;
		}
	}

	Test = GOOD;
}
