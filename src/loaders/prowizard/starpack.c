/*
 *  StarTrekker _Packer.c   1997 (c) Asle / ReDoX
 *
 * Converts back to ptk StarTrekker packed MODs
 *
*/

#include <string.h>
#include <stdlib.h>

void Depack_STARPACK (FILE * in, FILE * out)
{
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00, c5;
	uint8 pnum[128];
	uint8 pnum_tmp[128];
	uint8 pat_pos;
	uint8 *tmp;
	uint8 Pattern[1024];
	uint8 PatMax = 0x00;
	long i = 0, j = 0, k = 0;
	long ssize = 0;
	long paddr[128];
	long paddr_tmp[128];
	long paddr_tmp2[128];
	long tmp_ptr, tmp1, tmp2;
	long sdataAddress = 0;
	// FILE *in,*out;

	if (Save_Status == BAD)
		return;

	memset(pnum, 0, 128);
	memset(pnum_tmp, 0, 128);
	memset(paddr, 0, 128 * 4);
	memset(paddr_tmp, 0, 128 * 4);
	memset(paddr_tmp2, 0, 128 * 4);

	// in = fdopen (fd_in, "rb");
	// sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
	// out = fdopen (fd_out, "w+b");

	/* read and write title */
	for (i = 0; i < 20; i++) {	/* title */
		fread (&c1, 1, 1, in);
		fwrite (&c1, 1, 1, out);
	}

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
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* read size of pattern table */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	pat_pos = ((c1 << 8) + c2) / 4;
	/*printf ( "Size of pattern table : %d\n" , pat_pos ); */

	/* bypass $0000 unknown bytes */
	fseek (in, 2, 1);	/* SEEK_CUR */

/***********/

	for (i = 0; i < 128; i++) {
		fread (&c1, 1, 1, in);
		fread (&c2, 1, 1, in);
		fread (&c3, 1, 1, in);
		fread (&c4, 1, 1, in);
		paddr[i] =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
	}

	/* ordering of patterns addresses */

	tmp_ptr = 0;
	for (i = 0; i < pat_pos; i++) {
		if (i == 0) {
			pnum[0] = 0x00;
			tmp_ptr++;
			continue;
		}

		for (j = 0; j < i; j++) {
			if (paddr[i] == paddr[j]) {
				pnum[i] = pnum[j];
				break;
			}
		}
		if (j == i)
			pnum[i] = tmp_ptr++;
	}
/*
for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%x," , pnum[i] );
fprintf ( info , "\n\n" );
*/
	/* correct re-order */
  /********************/
	for (i = 0; i < 128; i++)
		paddr_tmp[i] = paddr[i];

      restart:
	for (i = 0; i < pat_pos; i++) {
		for (j = 0; j < i; j++) {
			if (paddr_tmp[i] < paddr_tmp[j]) {
				tmp2 = pnum[j];
				pnum[j] = pnum[i];
				pnum[i] = tmp2;
				tmp1 = paddr_tmp[j];
				paddr_tmp[j] = paddr_tmp[i];
				paddr_tmp[i] = tmp1;
				goto restart;
			}
		}
	}
/*
for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%x," , pnum[i] );
fprintf ( info , "\n\n" );
*/
	j = 0;
	for (i = 0; i < 128; i++) {
		if (i == 0) {
			paddr_tmp2[j] = paddr_tmp[i];
			continue;
		}

		if (paddr_tmp[i] == paddr_tmp2[j])
			continue;
		paddr_tmp2[++j] = paddr_tmp[i];
	}

/*
for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%ld," , paddr[i] );
fprintf ( info , "\n\n" );
for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%ld," , paddr_tmp2[i] );
fprintf ( info , "\n\n" );

for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%x," , pnum_tmp[i] );
fprintf ( info , "\n\n" );
*/

	/* try to locate unused patterns .. hard ! */
	j = 0;
	for (i = 0; i < (pat_pos - 1); i++) {
/*
fprintf ( info , "%6ld (%6ld,%6ld)\n"
               , paddr_tmp2[i+1] - paddr_tmp2[i]
               , paddr_tmp2[i+1]
               , paddr_tmp2[i] );
*/
		paddr_tmp[j] = paddr_tmp2[i];
		j += 1;
		if ((paddr_tmp2[i + 1] - paddr_tmp2[i]) > 1024) {
			/*printf ( "! pattern %ld is not used ... saved anyway\n" , j ); */
			paddr_tmp[j] = paddr_tmp2[i] + 1024;
			j += 1;
		}
	}

	/* assign pattern list */
	for (c1 = 0x00; c1 < 128; c1++) {
		for (c2 = 0x00; c2 < 128; c2++)
			if (paddr[c1] == paddr_tmp[c2]) {
				pnum_tmp[c1] = c2;
				break;
			}
	}
/*
for ( i=0 ; i<128 ; i++ )
fprintf ( info , "%x," , pnum_tmp[i] );
fprintf ( info , "\n\n" );
*/

	memset(pnum, 0, 128);
	for (i = 0; i < pat_pos; i++) {
		pnum[i] = pnum_tmp[i];
	}

	/* write number of position */
	fwrite (&pat_pos, 1, 1, out);

	/* get highest pattern number */
	for (i = 0; i < pat_pos; i++)
		if (pnum[i] > PatMax)
			PatMax = pnum[i];

	/*printf ( "Highest pattern number : %d\n" , PatMax ); */

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* write pattern list */
	for (i = 0; i < 128; i++)
		fwrite (&pnum[i], 1, 1, out);

/***********/


	/* write ptk's ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fwrite (&c3, 1, 1, out);
	fwrite (&c2, 1, 1, out);

	/* read sample data address */
	fseek (in, 0x310, 0);
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	sdataAddress =
		(c1 << 24) + (c2 << 16) + (c3 << 8) + c4 +
		0x314;

	/* pattern data */
	fseek (in, 0x314, 0);	/* SEEK_CUR */
	PatMax += 1;
	for (i = 0; i < PatMax; i++) {
		memset(Pattern, 0, 1024);
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				fread (&c1, 1, 1, in);
				if (c1 == 0x80) {
					Pattern[j * 16 + k * 4] = 0x00;
					Pattern[j * 16 + k * 4 + 1] = 0x00;
					Pattern[j * 16 + k * 4 + 2] = 0x00;
					Pattern[j * 16 + k * 4 + 3] = 0x00;
					continue;
				}
				fread (&c2, 1, 1, in);
				fread (&c3, 1, 1, in);
				fread (&c4, 1, 1, in);
				Pattern[j * 16 + k * 4] = c1 & 0x0f;
				Pattern[j * 16 + k * 4 + 1] = c2;
				Pattern[j * 16 + k * 4 + 2] = c3 & 0x0f;
				Pattern[j * 16 + k * 4 + 3] = c4;

				c5 = (c1 & 0xf0) | ((c3 >> 4) & 0x0f);
				c5 /= 4;
				Pattern[j * 16 + k * 4] |= (c5 & 0xf0);
				Pattern[j * 16 + k * 4 + 2] |=
					((c5 << 4) & 0xf0);
			}
		}
		fwrite (Pattern, 1024, 1, out);
		/*printf ( "+" ); */
	}
	/*printf ( "\n" ); */

	/* sample data */
	fseek (in, sdataAddress, 0);
	tmp = (uint8 *) malloc (ssize);
	memset(tmp, 0, ssize);
	fread (tmp, ssize, 1, in);
	fwrite (tmp, ssize, 1, out);
	free (tmp);


	Crap ("STP:Startrekker Pack", BAD, BAD, out);

	fflush (in);
	fflush (out);

	printf ("done\n");
	return;			/* useless ... but */
}


void testSTARPACK (void)
{
	/* test 1 */
	if (i < 23) {
/*printf ( "#1 (i:%ld)\n" , i );*/
		Test = BAD;
		return;
	}

	/* test 2 */
	start = i - 23;
	l =
		(data[start + 268] << 8) +
		data[start + 269];
	k = l / 4;
	if ((k * 4) != l) {
/*printf ( "#2,0 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
	if (k > 127) {
/*printf ( "#2,1 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
	if (k == 0) {
/*printf ( "#2,2 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}

	if (data[start + 784] != 0) {
/*printf ( "#3,-1 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}

	/* test #3  smp size < loop start + loop size ? */
	/* l is still the size of the pattern list */
	for (k = 0; k < 31; k++) {
		j =
			(((data[start + 20 + k * 8] << 8) +
				 data[start + 21 +
					k * 8]) * 2);
		ssize =
			(((data[start + 24 + k * 8] << 8) +
				data[start + 25 +
					k * 8]) * 2) +
			(((data[start + 26 + k * 8] << 8) +
				data[start + 27 +
					k * 8]) * 2);
		if ((j + 2) < ssize) {
/*printf ( "#3 (Start:%ld)\n" , start );*/
			Test = BAD;
			ssize = 0;
			return;
		}
	}
	ssize = 0;

	/* test #4  finetunes & volumes */
	/* l is still the size of the pattern list */
	for (k = 0; k < 31; k++) {
		if ((data[start + 22 + k * 8] > 0x0f)
			|| (data[start + 23 + k * 8] > 0x40)) {
/*printf ( "#4 (Start:%ld)\n" , start );*/
			Test = BAD;
			return;
		}
	}

	/* test #5  pattern addresses > sample address ? */
	/* l is still the size of the pattern list */
	/* get sample data address */
	if ((start + 0x314) > in_size) {
/*printf ( "#5,-1 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
	/* k gets address of sample data */
	k = (data[start + 784] << 24)
		+ (data[start + 785] << 16)
		+ (data[start + 786] << 8)
		+ data[start + 787];
	if ((k + start) > in_size) {
/*printf ( "#5,0 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
	if (k < 788) {
/*printf ( "#5,1 (Start:%ld)\n" , start );*/
		Test = BAD;
		return;
	}
	/* k is the address of the sample data */
	/* pattern addresses > sample address ? */
	for (j = 0; j < l; j += 4) {
		/* m gets each pattern address */
		m =
			(data[start + 272 +
				 j] << 24) +
			(data[start + 273 + j] << 16)
			+ (data[start + 274 + j] << 8)
			+ data[start + 275 + j];
		if (m > k) {
/*printf ( "#5,2 (Start:%ld) (smp addy:%ld) (pat addy:%ld) (pat nbr:%ld) (max:%ld)\n"
         , start 
         , k
         , m
         , (j/4)
         , l );*/
			return;
		}
	}
	/* test last patterns of the pattern list == 0 ? */
	j += 2;
	while (j < 128) {
		m =
			(data[start + 272 +
				 j * 4] << 24) +
			(data[start + 273 +
				j * 4] << 16) +
			(data[start + 274 + j * 4] << 8)
			+ data[start + 275 + j * 4];
		if (m != 0) {
/*printf ( "#5,3 (start:%ld)\n" , start );*/
			return;
		}
		j += 1;
	}


	/* test pattern data */
	/* k is the address of the sample data */
	j = start + 788;
	/* j points on pattern data */
/*printf ( "j:%ld , k:%ld\n" , j , k );*/
	while (j < (k + start - 4)) {
		if (data[j] == 0x80) {
			j += 1;
			continue;
		}
		if (data[j] > 0x80) {
/*printf ( "#6 (start:%ld)\n" , start );*/
			return;
		}
		/* empty row ? ... not ptk_tableible ! */
		if ((data[j] == 0x00) &&
			(data[j + 1] == 0x00) &&
			(data[j + 2] == 0x00) &&
			(data[j + 3] == 0x00)) {
/*printf ( "#6,0 (start:%ld)\n" , start );*/
			return;
		}
		/* fx = C .. arg > 64 ? */
		if (((data[j + 2] * 0x0f) == 0x0C)
			&& (data[j + 3] > 0x40)) {
/*printf ( "#6,1 (start:%ld)\n" , start );*/
			return;
		}
		/* fx = D .. arg > 64 ? */
		if (((data[j + 2] * 0x0f) == 0x0D)
			&& (data[j + 3] > 0x40)) {
/*printf ( "#6,2 (start:%ld)\n" , start );*/
			return;
		}
		j += 4;
	}

	Test = GOOD;
}
