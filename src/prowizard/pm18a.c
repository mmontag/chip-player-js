/*
 * Promizer_18a.c   Copyright (C) 1997 Asle / ReDoX
 *		    Modified by Claudio Matsuoka
 *
 * Converts PM18a packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * claudio's note: this code asks for heavy optimization. maybe later
 *
 * $Id: pm18a.c,v 1.3 2007-09-30 00:08:19 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_p18a (uint8 *, int);
static int depack_p18a (FILE *, FILE *);

struct pw_format pw_p18a = {
	"P18A",
	"Promizer 1.8a",
	0x00,
	test_p18a,
	depack_p18a
};

#define ON  0
#define OFF 1

static int depack_p18a (FILE *in, FILE *out)
{
    uint8 c1, c2, c3, c4;
    short pat_max = 0;
    long tmp_ptr;
    short refmax = 0;
    uint8 pnum[128];
    long paddr[128];
    short pptr[64][256];
    uint8 NOP = 0x00;    /* number of pattern */
    uint8 *reftab;
    uint8 *sdata;
    uint8 pat[128][1024];
    long i = 0, j = 0, k = 0, l = 0;
    long ssize = 0;
    long psize = 0l;
    long SDAV = 0l;
    uint8 FLAG = OFF;
    uint8 fin[31];
    uint8 ptk_table[37][2];
    uint8 oins[4];
    short per;

    bzero (pnum, 128);
    bzero (pptr, 64 << 8);
    bzero (pat, 128 * 1024);
    bzero (fin, 31);
    bzero (oins, 4);
    bzero (paddr, 128 * 4);

    for (i = 0; i < 20; i++)    /* title */
	write8(out, 0);

    /* bypass replaycode routine */
    fseek (in, 4464, 0);    /* SEEK_SET */

    for (i = 0; i < 31; i++) {
	for (j = 0; j < 22; j++)    /*sample name */
	    write8(out, 0);

	fread (&c1, 1, 1, in);    /* size */
	fread (&c2, 1, 1, in);
	ssize += (((c1 << 8) + c2) * 2);
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fread (&c1, 1, 1, in);    /* finetune */
	fin[i] = c1;
	fwrite (&c1, 1, 1, out);
	fread (&c1, 1, 1, in);    /* volume */
	fwrite (&c1, 1, 1, out);
	fread (&c1, 1, 1, in);    /* loop start */
	fread (&c2, 1, 1, in);
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	fread (&c1, 1, 1, in);    /* loop size */
	fread (&c2, 1, 1, in);
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
    }

    /* pattern table lenght */
    fread (&c1, 1, 1, in);
    fread (&c2, 1, 1, in);
    NOP = ((c1 << 8) + c2) / 4;
    fwrite (&NOP, 1, 1, out);

    /*printf ( "Number of patterns : %d\n" , NOP ); */

    /* NoiseTracker restart byte */
    c1 = 0x7f;
    fwrite (&c1, 1, 1, out);

    for (i = 0; i < 128; i++)
	paddr[i] = read32b(in);

    /* ordering of patterns addresses */

    tmp_ptr = 0;
    for (i = 0; i < NOP; i++) {
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

    pat_max = tmp_ptr - 1;

    /* write pattern table */
    for (c1 = 0x00; c1 < 128; c1++) {
	fwrite (&pnum[c1], 1, 1, out);
    }

    c1 = 'M';
    c2 = '.';
    c3 = 'K';

    fwrite (&c1, 1, 1, out);
    fwrite (&c2, 1, 1, out);
    fwrite (&c3, 1, 1, out);
    fwrite (&c2, 1, 1, out);


    /* a little pre-calc code ... no other way to deal with these unknown
       pattern data sizes ! :( */
    fseek (in, 4460, 0);    /* SEEK_SET */
    fread (&c1, 1, 1, in);
    fread (&c2, 1, 1, in);
    fread (&c3, 1, 1, in);
    fread (&c4, 1, 1, in);
    psize =
	(c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
    /* go back to pattern data starting address */
    fseek (in, 5226, 0);    /* SEEK_SET */
    /* now, reading all pattern data to get the max value of note */
    for (j = 0; j < psize; j += 2) {
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	if (((c1 << 8) + c2) > refmax)
	    refmax = (c1 << 8) + c2;
    }
/*
  printf ( "* refmax = %d\n" , refmax );
  printf ( "* where : %ld\n" , ftell ( in ) );
*/
    /* read "reference Table" */
    refmax += 1;	/* coz 1st value is 0 ! */
    i = refmax * 4;    /* coz each block is 4 bytes long */
    reftab = (uint8 *) malloc (i);
    fread (reftab, i, 1, in);

    /* go back to pattern data starting address */
    fseek (in, 5226, 0);    /* SEEK_SET */
    /*printf ( "Highest pattern number : %d\n" , pat_max ); */


    k = 0;
    for (j = 0; j <= pat_max; j++) {
	fseek (in, paddr[j] + 5226, 0);
	for (i = 0; i < 64; i++) {
	    /* VOICE #1 */

	    fread (&c1, 1, 1, in);
	    k += 1;
	    fread (&c2, 1, 1, in);
	    k += 1;
	    pat[j][i * 16] = reftab[((c1 << 8) + c2) * 4];
	    pat[j][i * 16 + 1] = reftab[((c1 << 8) + c2) * 4 + 1];
	    pat[j][i * 16 + 2] = reftab[((c1 << 8) + c2) * 4 + 2];
	    pat[j][i * 16 + 3] = reftab[((c1 << 8) + c2) * 4 + 3];
	    c3 = ((pat[j][i * 16 + 2] >> 4) & 0x0f) |
		(pat[j][i * 16] & 0xf0);
	    if (c3 != 0x00) {
		oins[0] = c3;
	    }
	    per = ((pat[j][i * 16] & 0x0f) << 8) + pat[j][i * 16 + 1];
	    if ((per != 0)
		&& (fin[oins[0] - 1] !=
		    0x00)) {
		for (l = 0; l < 36; l++)
		    if (tun_table[fin[oins[0] - 1]][l] == per) {
			pat[j][i * 16] &= 0xf0;
			pat[j][i * 16] |= ptk_table[l + 1][0];
			pat[j][i * 16 + 1] = ptk_table[l + 1][1];
			break;
		    }
	    }

	    if (((pat[j][i * 16 + 2] & 0x0f) == 0x0d) ||
		((pat[j][i * 16 + 2] & 0x0f) == 0x0b)) {
		FLAG = ON;
	    }

	    /* VOICE #2 */

	    fread (&c1, 1, 1, in);
	    k += 1;
	    fread (&c2, 1, 1, in);
	    k += 1;
	    pat[j][i * 16 + 4] = reftab[((c1 << 8) + c2) * 4];
	    pat[j][i * 16 + 5] = reftab[((c1 << 8) + c2) * 4 + 1];
	    pat[j][i * 16 + 6] = reftab[((c1 << 8) + c2) * 4 + 2];
	    pat[j][i * 16 + 7] = reftab[((c1 << 8) + c2) * 4 + 3];
	    c3 = ((pat[j][i * 16 + 6] >> 4) & 0x0f) |
		(pat[j][i * 16 + 4] & 0xf0);
	    if (c3 != 0x00) {
		oins[1] = c3;
	    }
	    per = ((pat[j][i * 16 + 4] & 0x0f) << 8) + pat[j][i * 16 + 5];
	    if ((per != 0) && (fin[oins[1] - 1] != 0x00)) {
		for (l = 0; l < 36; l++)
		    if (tun_table[fin[oins[1] - 1]][l] == per) {
			pat[j][i * 16 + 4] &= 0xf0;
			pat[j][i * 16 + 4] |= ptk_table[l + 1][0];
			pat[j][i * 16 + 5] = ptk_table[l + 1][1];
			break;
		    }
	    }

	    if (((pat[j][i * 16 + 6] & 0x0f) == 0x0d) ||
		((pat[j][i * 16 + 6] & 0x0f) == 0x0b)) {
		FLAG = ON;
	    }

	    /* VOICE #3 */

	    fread (&c1, 1, 1, in);
	    k += 1;
	    fread (&c2, 1, 1, in);
	    k += 1;
	    pat[j][i * 16 + 8] = reftab[((c1 << 8) + c2) * 4];
	    pat[j][i * 16 + 9] = reftab[((c1 << 8) + c2) * 4 + 1];
	    pat[j][i * 16 + 10] = reftab[((c1 << 8) + c2) * 4 + 2];
	    pat[j][i * 16 + 11] = reftab[((c1 << 8) + c2) * 4 + 3];
	    c3 = ((pat[j][i * 16 + 10] >> 4) & 0x0f) |
		(pat[j][i * 16 + 8] & 0xf0);
	    if (c3 != 0x00) {
		oins[2] = c3;
	    }
	    per = ((pat[j][i * 16 + 8] & 0x0f) << 8) + pat[j][i * 16 + 9];
	    if ((per != 0)
		&& (fin[oins[2] - 1] != 0x00)) {
		for (l = 0; l < 36; l++)
		    if (tun_table[fin[oins[2] - 1]][l] == per) {
			pat[j][i * 16 + 8] &= 0xf0;
			pat[j][i * 16 + 8] |= ptk_table[l + 1][0];
			pat[j][i * 16 + 9] = ptk_table[l + 1][1];
			break;
		    }
	    }

	    if (((pat[j][i * 16 + 10] & 0x0f) == 0x0d) ||
		((pat[j][i * 16 + 10] & 0x0f) == 0x0b)) {
		FLAG = ON;
	    }

	    /* VOICE #4 */

	    fread (&c1, 1, 1, in);
	    k += 1;
	    fread (&c2, 1, 1, in);
	    k += 1;
	    pat[j][i * 16 + 12] = reftab[((c1 << 8) + c2) * 4];
	    pat[j][i * 16 + 13] = reftab[((c1 << 8) + c2) * 4 + 1];
	    pat[j][i * 16 + 14] = reftab[((c1 << 8) + c2) * 4 + 2];
	    pat[j][i * 16 + 15] = reftab[((c1 << 8) + c2) * 4 + 3];
	    c3 = ((pat[j][i * 16 + 14] >> 4) & 0x0f) |
		(pat[j][i * 16 + 12] & 0xf0);
	    if (c3 != 0x00) {
		oins[3] = c3;
	    }
	    per = ((pat[j][i * 16 + 12] & 0x0f) << 8) + pat[j][i * 16 + 13];
	    if ((per != 0)
		&& (fin[oins[3] - 1] != 0x00)) {
		for (l = 0; l < 36; l++)
		    if (tun_table[fin[oins[3] - 1]][l] == per) {
			pat[j][i * 16 + 12] &= 0xf0;
			pat[j][i * 16 + 12] |= ptk_table[l + 1][0];
			pat[j][i * 16 + 13] = ptk_table[l + 1][1];
			break;
		    }
	    }

	    if (((pat[j][i * 16 + 14] & 0x0f) == 0x0d) ||
		((pat[j][i * 16 + 14] & 0x0f) == 0x0b)) {
		FLAG = ON;
	    }

	    if (FLAG == ON) {
		FLAG = OFF;
		break;
	    }
	}
	fwrite (pat[j], 1024, 1, out);
    }

    /*printf ( "Highest value in pattern data : %d\n" , refmax ); */

    free (reftab);

    fseek (in, 4456, 0);    /* SEEK_SET */
    fread (&c1, 1, 1, in);
    fread (&c2, 1, 1, in);
    fread (&c3, 1, 1, in);
    fread (&c4, 1, 1, in);
    SDAV = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
    fseek (in, 4460 + SDAV, 0);    /* SEEK_SET */


    /* Now, it's sample data ... though, VERY quickly handled :) */
    /* thx GCC ! (GNU C COMPILER). */

    /*printf ( "out: where before saving sample data : %ld\n" , ftell ( out ) ); */
    /*printf ( "Total sample size : %ld\n" , ssize ); */
    sdata = (uint8 *) malloc (ssize);
    fread (sdata, ssize, 1, in);
    fwrite (sdata, ssize, 1, out);
    free (sdata);

    return 0;
}


static int test_p18a (uint8 *data, int s)
{
	int i = 0, j, k, l;
	int start = 0;

	/* test 1 */
	PW_REQUEST_DATA (s, 22);

	if (data[i] != 0x60 || data[i + 1] != 0x38 || data[i + 2] != 0x60 ||
		data[i + 3] != 0x00 || data[i + 4] != 0x00 ||
		data[i + 5] != 0xa0 || data[i + 6] != 0x60 ||
		data[i + 7] != 0x00 || data[i + 8] != 0x01 ||
		data[i + 9] != 0x3e || data[i + 10] != 0x60 ||
		data[i + 11] != 0x00 || data[i + 12] != 0x01 ||
		data[i + 13] != 0x0c || data[i + 14] != 0x48 ||
		data[i + 15] != 0xe7) return -1;

	/* test 2 */
	if (data[start + 21] != 0xd2)
		return -1;

	PW_REQUEST_DATA (s, 4456);

	/* test 3 */
	j = (data[start + 4456] << 24) + (data[start + 4457] << 16) +
		(data[start + 4458] << 8) + data[start + 4459];

#if 0
	if ((start + j + 4456) > in_size) {
		Test = BAD;
		return;
	}
#endif

	/* test 4 */
	k = (data[start + 4712] << 8) + data[start + 4713];
	l = k / 4;
	l *= 4;
	if (l != k)
		return -1;

	/* test 5 */
	if (data[start + 36] != 0x11)
		return -1;

	/* test 6 */
	if (data[start + 37] != 0x00)
		return -1;

	return 0;
}
