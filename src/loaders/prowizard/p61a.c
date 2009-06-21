/*
 * The_Player_6.1a.c	1998 (c) Asle / ReDoX
 *			Modified by Claudio Matsuoka
 *
 * The Player 6.1a to Protracker.
 *
 * note: As for version 5.0A and 6.0A, it's a REAL mess !.
 *      It's VERY badly coded, I know. Just dont forget it was mainly done
 *      to test the description I made of P61a format.
 *      I certainly wont dare to beat Gryzor on the ground :). His Prowiz IS
 *      the converter to use !!!. Though, using the official depacker could
 *      be a good idea too :).
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_p61a (uint8 *, int);
static int depack_p61a (FILE *, FILE *);

struct pw_format pw_p61a = {
    "P61A",
    "The Player 6.1a",
    0x00,
    test_p61a,
    depack_p61a
};

#define ON 1
#define OFF 2

static int depack_p61a(FILE * in, FILE * out)
{
    uint8 c1, c2, c3, c4, c5, c6;
    long Max;
    uint8 *tmp;
    signed char *insDataWork;
    uint8 PatPos = 0x00;
    uint8 PatMax = 0x00;
    uint8 nins = 0x00;
    uint8 tdata[512][256];
    uint8 ptable[128];
    uint8 isize[31][2];
    uint8 PACK[31];
/*  uint8 DELTA[31];*/
    uint8 GLOBAL_DELTA = OFF;
    uint8 GLOBAL_PACK = OFF;
    long taddr[128][4];
    long tdata_addr = 0;
    long sdata_addr = 0;
    long ssize = 0;
    long i = 0, j, k, l, a, b, z;
    long smp_size[31];
    long saddr[32];
    long Unpacked_Sample_Data_Size;
    // FILE *in,*out;

    memset(taddr, 0, 128 * 4 * 4);
    memset(tdata, 0, 512 << 8);
    memset(ptable, 0, 128);
    memset(smp_size, 0, 31 * 4);
    memset(saddr, 0, 32 * 4);
    memset(isize, 0, 31 * 2);
    for (i = 0; i < 31; i++) {
	PACK[i] = OFF;
/*    DELTA[i] = OFF;*/
    }

//#include "ptktable.h"

    // in = fdopen (fd_in, "rb");
    // sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
    // out = fdopen (fd_out, "w+b");

    /* read sample data address */
    fread (&c1, 1, 1, in);
    fread (&c2, 1, 1, in);
    sdata_addr = (c1 << 8) + c2;

    /* read Real number of pattern */
    fread (&PatMax, 1, 1, in);

    /* read number of samples */
    fread (&nins, 1, 1, in);
    if ((nins & 0x80) == 0x80) {
	/*printf ( "Samples are saved as delta values !\n" ); */
	GLOBAL_DELTA = ON;
    }
    if ((nins & 0x40) == 0x40) {
	/*printf ( "some samples are packed !\n" ); */
	/*printf ( "\n! Since I could not understand the packing method of the\n" */
	/*         "! samples, neither could I do a depacker .. . mission ends here :)\n" ); */
	GLOBAL_PACK = ON;

	return -1;
    }
    nins &= 0x3F;

    /* read unpacked sample data size */
    if (GLOBAL_PACK == ON) {
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
	fread (&c3, 1, 1, in);
	fread (&c4, 1, 1, in);
	Unpacked_Sample_Data_Size =
	    (c1 << 24) + (c2 << 16) +
	    (c3 << 8) + c4;
    }

    /* write title */
    tmp = (uint8 *) malloc (21);
    memset(tmp, 0, 21);
    fwrite (tmp, 20, 1, out);
    free (tmp);

    /* sample headers stuff */
    for (i = 0; i < nins; i++) {
	/* write sample name */
	c1 = 0x00;
	for (j = 0; j < 22; j++)
	    fwrite (&c1, 1, 1, out);

	/* sample size */
	fread (&isize[i][0], 1, 1, in);
	fread (&isize[i][1], 1, 1, in);
	j = (isize[i][0] << 8) + isize[i][1];
	if (j > 0xFF00) {
	    smp_size[i] = smp_size[0xFFFF - j];
	    isize[i][0] = isize[0xFFFF - j][0];
	    isize[i][1] = isize[0xFFFF - j][1];
/*fprintf ( debug , "!%2ld!" , 0xFFFF-j );*/
	    saddr[i + 1] = saddr[0xFFFF - j + 1];    /* - smp_size[i]+smp_size[0xFFFF-j]; */
	} else {
	    saddr[i + 1] =
	        saddr[i] + smp_size[i - 1];
	    smp_size[i] = j * 2;
	    ssize += smp_size[i];
	}
	j = smp_size[i] / 2;
	fwrite (&isize[i][0], 1, 1, out);
	fwrite (&isize[i][1], 1, 1, out);

	/* finetune */
	fread (&c1, 1, 1, in);
	if ((c1 & 0x40) == 0x40)
	    PACK[i] = ON;
	c1 &= 0x3F;
	fwrite (&c1, 1, 1, out);

	/* volume */
	fread (&c1, 1, 1, in);
	fwrite (&c1, 1, 1, out);

	/* loop start */
	fread (&c1, 1, 1, in);
	fread (&c2, 1, 1, in);
/*fprintf ( debug , "loop start : %2x, %2x " , c1,c2 );*/
	if ((c1 == 0xFF) && (c2 == 0xFF)) {
	    c3 = 0x00;
	    c4 = 0x01;
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c4, 1, 1, out);
/*fprintf ( debug , " <--- no loop! (%2x,%2x)\n" ,c3,c4);*/
	    continue;
	}
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
	l = j - ((c1 << 8) + c2);
/*fprintf ( debug , " -> size:%6ld  lstart:%5d  -> lsize:%ld\n" , j,c1*256+c2,l );*/

	/* WARNING !!! WORKS ONLY ON 80X86 computers ! */
	/* 68k machines code : c1 = *(tmp+2); */
	/* 68k machines code : c2 = *(tmp+3); */
	tmp = (uint8 *) & l;
	c1 = *(tmp + 1);
	c2 = *tmp;
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
    }

    /* go up to 31 samples */
    tmp = (uint8 *) malloc (30);
    memset(tmp, 0, 30);
    tmp[29] = 0x01;
    while (i != 31) {
	fwrite (tmp, 30, 1, out);
	i += 1;
    }
    free (tmp);
    /*printf ( "Whole sample size : %ld\n" , ssize ); */

/*fprintf ( debug , "Where after sample headers : %x\n" , ftell ( in ) );*/

    /* read tracks addresses per pattern */
    for (i = 0; i < PatMax; i++) {
/*fprintf ( debug , "\npattern %ld : " , i );*/
	for (j = 0; j < 4; j++) {
	    fread (&c1, 1, 1, in);
	    fread (&c2, 1, 1, in);
	    taddr[i][j] = (c1 << 8) + c2;
/*fprintf ( debug , "%6ld, " , taddr[i][j] );*/
	}
    }
/*fprintf ( debug , "\n\nwhere after the track addresses : %x\n\n" , ftell ( in ));*/


    /* pattern table */
/*fprintf ( debug , "\nPattern table :\n" );*/
    for (PatPos = 0; PatPos < 128; PatPos++) {
	fread (&c1, 1, 1, in);
	if (c1 == 0xFF)
	    break;
	ptable[PatPos] = c1;    /* <--- /2 in p50a */
/*fprintf ( debug , "%2x, " , ptable[PatPos] );*/
    }

    /* write size of pattern list */
    fwrite (&PatPos, 1, 1, out);
/*fprintf ( debug , "\nsize of the pattern table : %d\n\n" , PatPos );*/

    /* write noisetracker byte */
    c1 = 0x7f;
    fwrite (&c1, 1, 1, out);

    /* write pattern table */
    fwrite (ptable, 128, 1, out);

    /* write ptk's ID */
    c1 = 'M';
    c2 = '.';
    c3 = 'K';
    fwrite (&c1, 1, 1, out);
    fwrite (&c2, 1, 1, out);
    fwrite (&c3, 1, 1, out);
    fwrite (&c2, 1, 1, out);

/*fprintf ( debug , "\n\nbefore reading track data : %x\n" , ftell ( in ) );*/
    tdata_addr = ftell (in);

    /* rewrite the track data */

    /*printf ( "sorting and depacking tracks data ... " ); */
    for (i = 0; i < PatMax; i++) {
	Max = 63;
	for (j = 0; j < 4; j++) {
	    fseek (in, taddr[i][j] + tdata_addr, 0);
	    for (k = 0; k <= Max; k++) {
	        fread (&c1, 1, 1, in);

	        /* case no fxt nor fxtArg  (3 bytes) */
	        if (((c1 & 0x70) == 0x70) && (c1 != 0xFF)
	            && (c1 != 0x7F)) {
	            fread (&c2, 1, 1, in);
	            c6 = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0e);
	            tdata[i * 4 + j][k * 4] =
	                (c2 & 0x10) | (ptk_table[c6 / 2][0]);
	            tdata[i * 4 + j][k * 4 + 1] = ptk_table[c6 / 2][1];
	            tdata[i * 4 + j][k * 4 + 2] = ((c2 << 4) & 0xf0);

	            if ((c1 & 0x80) == 0x80) {
	                fread (&c3, 1, 1, in);
	                if (c3 < 0x80) {
	                    k += c3;
	                    continue;
	                }
	                c4 = c3 - 0x80;

	                for (l = 0; l < c4; l++) {
	                    k += 1;
	                    tdata[i * 4 + j][k * 4] = (c2 & 0x10) |
					(ptk_table[c6 / 2] [0]);
	                    tdata[i * 4 + j][k * 4 + 1] = ptk_table[c6 / 2][1];
	                    tdata[i * 4 + j][k * 4 + 2] = ((c2 << 4) & 0xf0);
	                }
	            }
	            continue;
	        }
	        /* end of case no fxt nor fxtArg */

	        /* case no Sample number nor Relative not number */
	        if (((c1 & 0x70) == 0x60) && (c1 != 0xFF)) {
	            fread (&c2, 1, 1, in);

	            c6 = c1 & 0x0f;
	            if (c6 == 0x08)
	                c1 -= 0x08;
	            tdata[i * 4 + j][k * 4 + 2] = (c1 & 0x0f);
	            if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                c2 = (c2 > 0x7f) ? ((0x100 - c2) << 4) : c2;
	            tdata[i * 4 + j][k * 4 + 3] = c2;

	            if (c6 == 0x0D) {	/* PATTERN BREAK, track ends */
	                Max = k;
	                k = 9999l;
	                continue;
	            }
	            if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                Max = k;
	                k = 9999l;
	                continue;
	            }
	            if ((c1 & 0x80) == 0x80) {
	                fread (&c3, 1, 1, in);
	                if (c3 < 0x80) {	/* bypass c3 rows */
	                    k += c3;
	                    continue;
	                }
	                c4 = c3 - 0x80;		/* repeat current row */
	                for (l = 0; l < c4; l++) {
	                    k += 1;
	                    tdata[i * 4 + j][k * 4 + 2] = (c1 & 0x0f);
	                    tdata[i * 4 + j][k * 4 + 3] = c2;
	                }
	            }
	            continue;
	        }
	        /* end of case no Sample number nor Relative not number */

	        if (((c1 & 0x80) == 0x80) && (c1 != 0xFF)) {
	            fread (&c2, 1, 1, in);
	            fread (&c3, 1, 1, in);
	            fread (&c4, 1, 1, in);
	            c1 = c1 & 0x7F;
	            tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
					(ptk_table[c1 / 2][0]);
	            tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
	            c6 = c2 & 0x0f;
	            if (c6 == 0x08)
	                c2 -= 0x08;
	            tdata[i * 4 + j][k * 4 + 2] = c2;

	            if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
	            tdata[i * 4 + j][k * 4 + 3] = c3;
	            if (c6 == 0x0D) {	/* PATTERN BREAK, track ends */
	                Max = k;
	                k = 9999l;
	                continue;
	            }
	            if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                Max = k;
	                k = 9999l;
	                continue;
	            }

	            if (c4 < 0x80) {	/* bypass c4 rows */
	                k += c4;
	                continue;
	            }
	            c4 = c4 - 0x80;
	            for (l = 0; l < c4; l++) {	/* repeat row c4-0x80 times */
	                k += 1;
	                tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
	                    			(ptk_table[c1 / 2][0]);
	                tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
	                c6 = c2 & 0x0f;
	                if (c6 == 0x08)
	                    c2 -= 0x08;
	                tdata[i * 4 + j][k * 4 + 2] = c2;
	                if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                    c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
	                tdata[i * 4 + j][k * 4 + 3] = c3;
	            }
	            continue;
	        }


	        if ((c1 & 0x7F) == 0x7F) {
	            if ((c1 & 0x80) == 0x00) {	/* bypass 1 row */
	                /*k += 1; */
	                continue;
	            }
	            fread (&c2, 1, 1, in);
	            if (c2 < 0x40) {	/* bypass c2 rows */
	                k += c2;
	                continue;
	            }
	            c2 -= 0x40;
	            fread (&c3, 1, 1, in);
	            z = c3;
	            if (c2 >= 0x80) {
	                c2 -= 0x80;
	                fread (&c4, 1, 1, in);
	                z = (c3 << 8) + c4;
	            }
	            a = ftell (in);
	            c5 = c2;
	            fseek (in, -(z), 1);
	            for (l = 0; (l <= c5) && (k <= Max); l++, k++) {
	                fread (&c1, 1, 1, in);

	                /* case no fxt nor fxtArg  (3 bytes) */
	                if (((c1 & 0x70) == 0x70) && (c1 != 0xFF)
	                    			&& (c1 != 0x7F)) {
	                    fread (&c2, 1, 1, in);
	                    c6 = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0e);
	                    tdata[i * 4 + j][k * 4] = (c2 & 0x10) |
						(ptk_table[c6 / 2][0]);
	                    tdata[i * 4 + j][k * 4 + 1] = ptk_table[c6 / 2][1];
	                    tdata[i * 4 + j][k * 4 + 2] = ((c2 << 4) & 0xf0);

	                    if ((c1 & 0x80) == 0x80) {
	                        fread (&c3, 1, 1, in);
	                        if (c3 < 0x80) {	/* bypass c3 rows */
	                            k += c3;
	                            continue;
	                        }
	                        c4 = c3 - 0x80;	/* repeat row c3-0x80 times */
	                        for (b = 0; b < c4; b++) {
	                            k += 1;
	                            tdata[i * 4 + j][k * 4] = (c2 & 0x10) |
	                                	(ptk_table [c6 / 2][0]);
	                            tdata[i * 4 + j][k * 4 + 1] =
						ptk_table[c6 / 2][1];
	                            tdata[i * 4 + j][k * 4 + 2] =
	                                	((c2 << 4) & 0xf0);
	                        }
	                    }
	                    continue;
	                }
	                /* end of case no fxt nor fxtArg */

	                /* case no Sample number nor Relative not number */
	                if (((c1 & 0x60) == 0x60) && (c1 != 0xFF) && (c1 != 0x7F)) {
	                    fread (&c2, 1, 1, in);
	                    c6 = c1 & 0x0f;
	                    if (c6 == 0x08)
	                        c1 -= 0x08;
	                    tdata[i * 4 + j][k * 4 + 2] = (c1 & 0x0f);
	                    if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                            c2 = (c2 > 0x7f) ? ( (0x100 - c2) << 4) : c2;
	                    tdata[i * 4 + j][k * 4 + 3] = c2;

	                    if (c6 == 0x0D) {	/* PATTERN BREAK, track ends */
	                        Max = k;
	                        k = 9999l;
	                        continue;
	                    }
	                    if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                        Max = k;
	                        k = 9999l;
	                        continue;
	                    }

	                    if ((c1 & 0x80) == 0x80) {
	                        fread (&c3, 1, 1, in);
	                        if (c3 < 0x80) {	/* bypass c3 rows */
	                            k += c3;
	                            continue;
	                        }
	                        c4 = c3 - 0x80;	/* repeat row c3-0x80 times */
	                        for (b = 0; b < c4; b++) {
	                            k += 1;
	                            tdata[i * 4 + j][k * 4 + 2] = (c1 & 0x0f);
	                            tdata[i * 4 + j][k * 4 + 3] = c2;
	                        }
	                    }
	                    continue;
	                }
	                /* end of case no Sample number nor Relative not number */

	                if (((c1 & 0x80) == 0x80) && (c1 != 0xFF) && (c1 != 0x7F)) {
	                    fread (&c2, 1, 1, in);
	                    fread (&c3, 1, 1, in);
	                    fread (&c4, 1, 1, in);
	                    c1 = c1 & 0x7f;
	                    tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
	                        		(ptk_table[c1 / 2][0]);
	                    tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
	                    c6 = c2 & 0x0f;
	                    if (c6 == 0x08)
	                        c2 -= 0x08;
	                    tdata[i * 4 + j][k * 4 + 2] = c2;
	                    if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                            c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
	                    tdata[i * 4 + j][k * 4 + 3] = c3;
	                    if (c6 == 0x0D) {	/* PATTERN BREAK, track ends */
	                        Max = k;
	                        k = l = 9999l;
	                        continue;
	                    }
	                    if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	                        Max = k;
	                        k = l = 9999l;
	                        continue;
	                    }
	                    if (c4 < 0x80) {	/* bypass c4 rows */
	                        k += c4;
	                        continue;
	                    }
	                    c4 = c4 - 0x80;	/* repeat row c4-0x80 times */
	                    for (b = 0; b < c4; b++) {
	                        k += 1;
	                        tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
	                            		(ptk_table [c1 / 2] [0]);
	                        tdata[i * 4 + j][k * 4 + 1] =
						ptk_table[c1 / 2][1];
	                        c6 = c2 & 0x0f;
	                        if (c6 == 0x08)
	                                c2 -= 0x08;
	                        tdata[i * 4 + j][k * 4 + 2] = c2;
	                        if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                                c3 = (c3 > 0x7f) ?  ( (0x100 - c3) << 4) : c3;
	                        tdata[i * 4 + j][k * 4 + 3] = c3;
	                    }
	                    continue;
	                }
	                if ((c1 & 0x7F) == 0x7F) {
	                    if ((c1 & 0x80) == 0x00) {	/* bypass 1 row */
	                        /*k += 1; */
	                        continue;
	                    }
	                    fread (&c2, 1, 1, in);
	                    if (c2 < 0x40) {	/* bypass c2 rows */
	                        k += c2;
	                        continue;
	                    }
	                    continue;
	                }

	                fread(&c2, 1, 1, in);
	                fread(&c3, 1, 1, in);
	                tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
	                    			(ptk_table[c1 / 2][0]);
	                tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
	                c6 = c2 & 0x0f;
	                if (c6 == 0x08)
	                    c2 -= 0x08;
	                tdata[i * 4 + j][k * 4 + 2] = c2;
	                if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
	                    c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
	                tdata[i * 4 + j][k * 4 + 3] = c3;
	            }
	            fseek (in, a, 0);
	            k -= 1;
	            continue;
	        }


	        fread (&c2, 1, 1, in);
	        fread (&c3, 1, 1, in);
	        tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) |
					(ptk_table[c1 / 2][0]);
	        tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
	        c6 = c2 & 0x0f;
	        if (c6 == 0x08)
	            c2 -= 0x08;
	        tdata[i * 4 + j][k * 4 + 2] = c2;
	        if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
			c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
	        tdata[i * 4 + j][k * 4 + 3] = c3;
	        if (c6 == 0x0D) {	/* PATTERN BREAK, track ends */
	            Max = k;
	            k = 9999l;
	            continue;
	        }
	        if (c6 == 0x0B) {	/* PATTERN JUMP, track ends */
	            Max = k;
	            k = 9999l;
	            continue;
	        }
	    }
	}
    }

    /* write pattern data */

    /*printf ( "writing pattern data ... " ); */
    tmp = (uint8 *) malloc (1024);
    for (i = 0; i < PatMax; i++) {
	memset(tmp, 0, 1024);
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
	        tmp[j * 16 + k * 4] = tdata[k + i * 4][j * 4];
	        tmp[j * 16 + k * 4 + 1] = tdata[k + i * 4][j * 4 + 1];
	        tmp[j * 16 + k * 4 + 2] = tdata[k + i * 4][j * 4 + 2];
	        tmp[j * 16 + k * 4 + 3] = tdata[k + i * 4][j * 4 + 3];
	    }
	}
	fwrite (tmp, 1024, 1, out);
    }
    free (tmp);
    /*printf ( "ok\n" ); */


    /* go to sample data address */

    fseek (in, sdata_addr, 0);

    /* read and write sample data */

    /*printf ( "writing sample data ... " ); */
    for (i = 0; i < nins; i++) {
	fseek (in, sdata_addr + saddr[i + 1], 0);
	insDataWork = (signed char *) malloc (smp_size[i]);
	memset(insDataWork, 0, smp_size[i]);
	fread (insDataWork, smp_size[i], 1, in);
	if (GLOBAL_DELTA == ON) {
	    c1 = 0x00;
	    for (j = 1; j < smp_size[i]; j++) {
	        c2 = insDataWork[j];
	        c2 = 0x100 - c2;
	        c3 = c2 + c1;
	        insDataWork[j] = c3;
	        c1 = c3;
	    }
	}
	fwrite (insDataWork, smp_size[i], 1, out);
	free (insDataWork);
    }

    if (GLOBAL_DELTA == ON)
	pw_p60a.flags |= PW_DELTA;

    return 0;
}


static int test_p61a(uint8 *data, int s)
{
    int j, k, l, m, n, o;
    int start = 0, ssize;

#if 0
    if (i < 7) {
	Test = BAD;
	return;
    }
    start = i - 7;
#endif

    /* number of pattern (real) */
    /* m is the real number of pattern */
    m = data[start + 2];
    if (m > 0x7f || m == 0)
	return -1;

    /* number of sample */
    /* k is the number of sample */
    k = (data[start + 3] & 0x3F);
    if (k > 0x1F || k == 0)
	return -1;

    for (l = 0; l < k; l++) {
	/* test volumes */
	if (data[start + 7 + l * 6] > 0x40)
	    return -1;

	/* test finetunes */
	if (data[start + 6 + l * 6] > 0x0F)
	    return -1;
    }

    /* test sample sizes and loop start */
    ssize = 0;
    for (n = 0; n < k; n++) {
	o = ((data[start + 4 + n * 6] << 8) + data[start + 5 + n * 6]);
	if ((o < 0xFFDF && o > 0x8000) || o == 0)
	    return -1;

	if (o < 0xFF00)
	    ssize += (o * 2);

	j = ((data[start + 8 + n * 6] << 8) + data[start + 9 + n * 6]);
	if (j != 0xFFFF && j >= o)
	    return -1;

	if (o > 0xFFDF) {
	    if ((0xFFFF - o) > k)
	        return -1;
	}
    }

    /* test sample data address */
    /* j is the address of the sample data */
    j = (data[start] << 8) + data[start + 1];
    if (j < (k * 6 + 4 + m * 8))
	return -1;

    /* test track table */
    for (l = 0; l < (m * 4); l++) {
	o = ((data[start + 4 + k * 6 + l * 2] << 8) +
	    data[start + 4 + k * 6 + l * 2 + 1]);
	if ((o + k * 6 + 4 + m * 8) > j)
	    return -1;
    }

    /* test pattern table */
    l = 0;
    o = 0;
    /* first, test if we dont oversize the input file */
    PW_REQUEST_DATA (s, start + k * 6 + 4 + m * 8);

    while ((data[start + k * 6 + 4 + m * 8 + l] != 0xFF) && (l < 128)) {
	if (data[start + k * 6 + 4 + m * 8 + l] > (m - 1))
	    return -1;

	if (data[start + k * 6 + 4 + m * 8 + l] > o)
	    o = data[start + k * 6 + 4 + m * 8 + l];
	l++;
    }

    /* are we beside the sample data address ? */
    if ((k * 6 + 4 + m * 8 + l) > j)
	return -1;

    if (l == 0 || l == 128)
	return -1;

    o += 1;
    /* o is the highest number of pattern */

    /* test notes ... pfiew */

    PW_REQUEST_DATA (s, start + j + 1);

    l += 1;
    for (n = (k * 6 + 4 + m * 8 + l); n < j; n++) {
	if ((data[start + n] & 0xff) == 0xff) {
	    if ((data[start + n + 1] & 0xc0) == 0x00) {
	        n += 1;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) == 0x40) {
	        n += 2;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) == 0xc0) {
	        n += 3;
	        continue;
	    }
	}
	if ((data[start + n] & 0xff) == 0x7f)
	    continue;

	/* no fxt nor fxtArg */
	if ((data[start + n] & 0xf0) == 0xf0) {
	    if ((data[start + n + 1] & 0x1F) > k)
	        return -1;
	    n += 2;
	    continue;
	}

	if ((data[start + n] & 0xf0) == 0x70) {
	    if ((data[start + n + 1] & 0x1F) > k)
	        return -1;
	    n += 1;
	    continue;
	}

	/* no note nor Sample number */
	if ((data[start + n] & 0xf0) == 0xe0) {
	    n += 2;
	    continue;
	}

	if ((data[start + n] & 0xf0) == 0x60) {
	    n += 1;
	    continue;
	}

	if ((data[start + n] & 0x80) == 0x80) {
	    if ((((data[start + n] << 4) & 0x10)
	        | ((data[start + n + 1] >> 4) & 0x0F)) > k)
	        return -1;
	    n += 3;
	    continue;
	}

	if ((((data[start + n] << 4) & 0x10) |
	    ((data[start + n + 1] >> 4) & 0x0F)) > k)
	    return -1;
	n += 2;
    }

    /* ssize is the whole sample data size */
    /* j is the address of the sample data */
    
    return 0;
}


#if 0
/******************/
/* packed samples */
/******************/
void testP61A_pack (void)
{
    if (i < 11) {
	Test = BAD;
	return;
    }
    start = i - 11;

    /* number of pattern (real) */
    m = data[start + 2];
    if ((m > 0x7f) || (m == 0)) {
/*printf ( "#1 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    /* m is the real number of pattern */

    /* number of sample */
    k = data[start + 3];
    if ((k & 0x40) != 0x40) {
/*printf ( "#2,0 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    k &= 0x3F;
    if ((k > 0x1F) || (k == 0)) {
/*printf ( "#2,1 Start:%ld (k:%ld)\n" , start,k );*/
	Test = BAD;
	return;
    }
    /* k is the number of sample */

    /* test volumes */
    for (l = 0; l < k; l++) {
	if (data[start + 11 + l * 6] > 0x40) {
/*printf ( "#3 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
    }

    /* test fines */
    for (l = 0; l < k; l++) {
	if ((data[start + 10 + l * 6] & 0x3F) > 0x0F) {
	    Test = BAD;
/*printf ( "#4 Start:%ld\n" , start );*/
	    return;
	}
    }

    /* test sample sizes and loop start */
    ssize = 0;
    for (n = 0; n < k; n++) {
	o = ((data[start + 8 + n * 6] << 8) +
	    data[start + 9 + n * 6]);
	if (((o < 0xFFDF) && (o > 0x8000)) || (o == 0)) {
/*printf ( "#5 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
	if (o < 0xFF00)
	    ssize += (o * 2);

	j = ((data[start + 12 + n * 6] << 8) +
	    data[start + 13 + n * 6]);
	if ((j != 0xFFFF) && (j >= o)) {
/*printf ( "#5,1 Start:%ld\n" , start );*/
	    Test = BAD;
	    return;
	}
	if (o > 0xFFDF) {
	    if ((0xFFFF - o) > k) {
/*printf ( "#5,2 Start:%ld\n" , start );*/
	        Test = BAD;
	        return;
	    }
	}
    }

    /* test sample data address */
    j =
	(data[start] << 8) + data[start +
	1];
    if (j < (k * 6 + 8 + m * 8)) {
/*printf ( "#6 Start:%ld\n" , start );*/
	Test = BAD;
	ssize = 0;
	return;
    }
    /* j is the address of the sample data */


    /* test track table */
    for (l = 0; l < (m * 4); l++) {
	o =
	    ((data[start + 8 + k * 6 +
	             l * 2] << 8) +
	    data[start + 8 + k * 6 + l * 2 +
	        1]);
	if ((o + k * 6 + 8 + m * 8) > j) {
/*printf ( "#7 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(o:%ld)\n"
, start
, (data[start+k*6+8+l*2]*256)+data[start+8+k*6+l*2+1]
, start+k*6+8+l*2
, l
, m
, o );*/
	    Test = BAD;
	    return;
	}
    }

    /* test pattern table */
    l = 0;
    o = 0;
    /* first, test if we dont oversize the input file */
    if ((k * 6 + 8 + m * 8) > in_size) {
/*printf ( "8,0 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    while ((data[start + k * 6 + 8 + m * 8 + l] !=
	    0xFF) && (l < 128)) {
	if (data[start + k * 6 + 8 + m * 8 +
	        l] > (m - 1)) {
/*printf ( "#8,1 Start:%ld (value:%ld)(where:%x)(l:%ld)(m:%ld)(k:%ld)\n"
, start
, data[start+k*6+8+m*8+l]
, start+k*6+8+m*8+l
, l
, m
, k );*/
	    Test = BAD;
	    ssize = 0;
	    return;
	}
	if (data[start + k * 6 + 8 + m * 8 +
	        l] > o)
	    o =
	        data[start + k * 6 + 8 +
	        m * 8 + l];
	l++;
    }
    if ((l == 0) || (l == 128)) {
/*printf ( "#8.2 Start:%ld\n" , start );*/
	Test = BAD;
	return;
    }
    o /= 2;
    o += 1;
    /* o is the highest number of pattern */


    /* test notes ... pfiew */
    l += 1;
    for (n = (k * 6 + 8 + m * 8 + l); n < j; n++) {
	if ((data[start + n] & 0xff) == 0xff) {
	    if ((data[start + n + 1] & 0xc0) ==
	        0x00) {
	        n += 1;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) ==
	        0x40) {
	        n += 2;
	        continue;
	    }
	    if ((data[start + n + 1] & 0xc0) ==
	        0xc0) {
	        n += 3;
	        continue;
	    }
	}
	if ((data[start + n] & 0xff) == 0x7f) {
	    continue;
	}

	/* no fxt nor fxtArg */
	if ((data[start + n] & 0xf0) == 0xf0) {
	    if ((data[start + n + 1] & 0x1F) >
	        k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 2;
	    continue;
	}
	if ((data[start + n] & 0xf0) == 0x70) {
	    if ((data[start + n + 1] & 0x1F) >
	        k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 1;
	    continue;
	}
	/* no note nor Sample number */
	if ((data[start + n] & 0xf0) == 0xe0) {
	    n += 2;
	    continue;
	}
	if ((data[start + n] & 0xf0) == 0x60) {
	    n += 1;
	    continue;
	}

	if ((data[start + n] & 0x80) == 0x80) {
	    if ((((data[start +
	                        n] << 4) &
	                0x10) |
	            ((data[start + n +
	                        1] >> 4) &
	                0x0F)) > k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	        Test = BAD;
	        return;
	    }
	    n += 3;
	    continue;
	}

	if ((((data[start +
	                    n] << 4) & 0x10) |
	        ((data[start + n +
	                    1] >> 4) & 0x0F)) >
	    k) {
/*printf ( "#9,1 Start:%ld (value:%ld) (where:%x) (n:%ld) (j:%ld)\n"
, start
, data[start+n]
, start+n
, n
, j
 );*/
	    Test = BAD;
	    return;
	}
	n += 2;
    }


    /* ssize is the whole sample data size */
    /* j is the address of the sample data */
    Test = GOOD;
}
#endif
