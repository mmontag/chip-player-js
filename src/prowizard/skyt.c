/*
 *   Skyt_Packer.c   1997 (c) Asle / ReDoX
 *
 * Converts back to ptk SKYT packed MODs
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 *
*/

#include <string.h>
#include <stdlib.h>

void Depack_SKYT (FILE * in, FILE * out)
{
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 ptable[128];
	uint8 pat_pos;
	uint8 Pattern[1024];
	uint8 ptk_table[37][2];
	long i = 0, j = 0, k = 0;
	long ssize = 0;
	long Track_Values[128][4];
	long Track_Address;
	long Where = start;	/* main pointer to prevent fread() */
	// FILE *out;

#include "ptktable.h"

	if (Save_Status == BAD)
		return;

	memset(ptable, 0, 128);
	memset(Track_Values, 0, 128 * 4);

	// sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
	// out = fdopen (fd_out, "w+b");

	/* write title */
	for (i = 0; i < 20; i++)	/* title */
		fwrite (&c1, 1, 1, out);

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {
		c1 = 0x00;
		for (j = 0; j < 22; j++)	/*sample name */
			fwrite (&c1, 1, 1, out);

		c1 = data[Where++];
		c2 = data[Where++];
		ssize += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
		fwrite (&data[Where++], 1, 1, out);	/* finetune */
		fwrite (&data[Where++], 1, 1, out);	/* volume */
		fwrite (&data[Where++], 1, 1, out);	/* loop start */
		fwrite (&data[Where++], 1, 1, out);
		c1 = data[Where++];	/* loop size */
		c2 = data[Where++];
		if ((c1 == 0x00) && (c2 == 0x00))
			c2 = 0x01;
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* bypass 8 empty bytes and bypass "SKYT" ID */
	Where += 12;

	/* pattern table lenght */
	pat_pos = data[Where++];
	pat_pos += 1;
	fwrite (&pat_pos, 1, 1, out);
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* write NoiseTracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* read track numbers ... and deduce pattern list */
	for (i = 0; i < pat_pos; i++) {
		for (j = 0; j < 4; j++) {
			c1 = data[Where++];
			c2 = data[Where++];
			Track_Values[i][j] = (c1 << 8) + c2;
		}
	}

	/* write pseudo pattern list */
	for (c1 = 0x00; c1 < pat_pos; c1 += 0x01) {
		fwrite (&c1, 1, 1, out);
	}
	c2 = 0x00;
	while (c1 != 128) {
		fwrite (&c2, 1, 1, out);
		c1 += 0x01;
	}

	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* bypass $00 unknown byte */
	Where += 1;

	/* get track address */
	Track_Address = Where;

	/* track data */
	for (i = 0; i < pat_pos; i++) {
		memset(Pattern, 0, 1024);
		for (j = 0; j < 4; j++) {
			Where =
				start + Track_Address +
				(Track_Values[i][j] - 1) << 8;
			for (k = 0; k < 64; k++) {
				c1 = data[Where++];
				c2 = data[Where++];
				c3 = data[Where++];
				c4 = data[Where++];
				Pattern[k * 16 + j * 4] = c2 & 0xf0;
				Pattern[k * 16 + j * 4] |= ptk_table[c1][0];
				Pattern[k * 16 + j * 4 + 1] = ptk_table[c1][1];
				Pattern[k * 16 + j * 4 + 2] =
					(c2 << 4) & 0xf0;
				Pattern[k * 16 + j * 4 + 2] |= c3;
				Pattern[k * 16 + j * 4 + 3] = c4;
			}
		}
		fwrite (Pattern, 1024, 1, out);
		/*printf ( "+" ); */
	}
	/*printf ( "\n" ); */

	/* sample data */
	fwrite (&data[Where], ssize, 1, out);

	/* crap */
	Crap ("SKYT:SKYT Packer", BAD, BAD, out);

	fflush (out);

	printf ("done\n");
	return;			/* useless ... but */
}

void testSKYT (void)
{
	/* test 1 */
	if (i < 256) {
		Test = BAD;
		return;
	}

	/* test 2 */
	start = i - 256;
	for (j = 0; j < 31; j++) {
		if (data[start + 4 + 8 * j] > 0x40) {
			Test = BAD;
			return;
		}
	}

	Test = GOOD;
}
