/*
 * The_Player_6.0a.c   Copyright (C) 1998 Asle / ReDoX
 *		       Modified by Claudio Matsuoka
 *
 * The Player 6.0a to Protracker.
 *
 * note: It's a REAL mess !. It's VERY badly coded, I know. Just dont forget
 *      it was mainly done to test the description I made of P60a format. I
 *      certainly wont dare to beat Gryzor on the ground :). His Prowiz IS
 *      the converter to use !!!. Though, using the official depacker could
 *      be a good idea too :).
 *
 * claudio's note: I don't care if it's a mess. My attempt to write a p60a
 *	depacker produced cleaner code that didn't work (tp_load.c).
 *
 * $Id: p60a_load.c,v 1.2 2003-06-24 23:31:41 dmierzej Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

/* #include "prowiz.h" */

#include "load.h"
#include "period.h"


static int test_p60a (uint8 *, int);
/* static int depack_p60a (FILE *, FILE *); */

#if 0
struct pw_format pw_p60a = {
	"P60A",
	"The Player 6.0a",
	0x00,
	test_p60a,
	NULL,
	depack_p60a
};
#endif

#define ON 1
#define OFF 2


static const uint8 ptk_table[37][2] = {
	{ 0x00, 0x00 },

	{ 0x03, 0x58 },
        { 0x03, 0x28 },
        { 0x02, 0xfa },
        { 0x02, 0xd0 },
        { 0x02, 0xa6 },
        { 0x02, 0x80 },   /*  1  */
        { 0x02, 0x5c },
        { 0x02, 0x3a },
        { 0x02, 0x1a },
        { 0x01, 0xfc },
        { 0x01, 0xe0 },
        { 0x01, 0xc5 },

        { 0x01, 0xac },
        { 0x01, 0x94 },
        { 0x01, 0x7d },
        { 0x01, 0x68 },
        { 0x01, 0x53 },
        { 0x01, 0x40 },   /*  2  */
        { 0x01, 0x2e },
        { 0x01, 0x1d },
        { 0x01, 0x0d },
        { 0x00, 0xfe },
        { 0x00, 0xf0 },
        { 0x00, 0xe2 },

        { 0x00, 0xd6 },
        { 0x00, 0xca },
        { 0x00, 0xbe },
        { 0x00, 0xb4 },
        { 0x00, 0xaa },
        { 0x00, 0xa0 },   /*  3  */
        { 0x00, 0x97 },
        { 0x00, 0x8f },
        { 0x00, 0x87 },
        { 0x00, 0x7f },
        { 0x00, 0x78 },
        { 0x00, 0x71 }
};


int p60a_load (FILE *f)
{
    uint8 c1, c2, c3, c4, c5, c6;
    long Max;
    uint8 *tmp;
    //signed char *insDataWork;
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
    long i = 0, j, k, l, a, b;
    long smp_size[31];
    long saddr[32];
    long unpacked_ssize;
#define BSIZE 8192 
    uint8 buffer[BSIZE];
    struct xxm_event *event;

    LOAD_INIT ();

    fread (buffer, 1, BSIZE, f);
    if (test_p60a (buffer, BSIZE) != 0)
	return -1;

    fseek (f, 0, SEEK_SET);
    
    memset (taddr, 0, 128 * 4 * 4);
    memset (tdata, 0, 512 << 8);
    memset (ptable, 0, 128);
    memset (smp_size, 0, 31 * 4);
    memset (saddr, 0, 32 * 4);
    memset (isize, 0, 31 * 2);

    for (i = 0; i < 31; i++) {
	PACK[i] = OFF;
/*    DELTA[i] = OFF;*/
    }

    /* read sample data address */
    fread (&c1, 1, 1, f);
    fread (&c2, 1, 1, f);
    sdata_addr = (c1 << 8) + c2;

    /* read Real number of pattern */
    fread (&PatMax, 1, 1, f);

    /* read number of samples */
    fread (&nins, 1, 1, f);
    if ((nins & 0x80) == 0x80) {
	/*printf ( "Samples are saved as delta values !\n" ); */
	GLOBAL_DELTA = ON;
    }
    if ((nins & 0x40) == 0x40) {
	/*printf ( "some samples are packed !\n" ); */
	/*printf ( "\n! Since I could not understand the packing method of the\n" */
	/*	 "! samples, neither could I do a depacker .. . mission ends here :)\n" ); */
	GLOBAL_PACK = ON;

	return -1;
    }
    nins &= 0x3F;

    /* read unpacked sample data size */
    if (GLOBAL_PACK == ON) {
	fread (&c1, 1, 1, f);
	fread (&c2, 1, 1, f);
	fread (&c3, 1, 1, f);
	fread (&c4, 1, 1, f);
	unpacked_ssize = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;
    }

#if 0
    /* write title */
    tmp = (uint8 *) malloc (21);
    bzero (tmp, 21);
    fwrite (tmp, 20, 1, out);
    free (tmp);
#endif

    sprintf (xmp_ctl->type, "The Player 6.0A");
    MODULE_INFO ();
    INSTRUMENT_INIT ();

    /* sample headers stuff */
    for (i = 0; i < nins; i++) {
        xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	/* write sample name */
#if 0
	c1 = 0x00;
	for (j = 0; j < 22; j++)
	    fwrite (&c1, 1, 1, out);
#endif

	/* sample size */
	fread (&isize[i][0], 1, 1, f);
	fread (&isize[i][1], 1, 1, f);
	j = (isize[i][0] << 8) + isize[i][1];
	if (j > 0xFF00) {
	    smp_size[i] = smp_size[0xFFFF - j];
	    isize[i][0] = isize[0xFFFF - j][0];
	    isize[i][1] = isize[0xFFFF - j][1];
	    saddr[i + 1] = saddr[0xFFFF - j + 1];
	} else {
	    saddr[i + 1] =
		saddr[i] + smp_size[i - 1];
	    smp_size[i] = j * 2;
	    ssize += smp_size[i];
	}
	j = smp_size[i] / 2;
#if 0
	fwrite (&isize[i][0], 1, 1, out);
	fwrite (&isize[i][1], 1, 1, out);
#endif

        xxs[i].len = smp_size[i];

	/* finetune */
	fread (&c1, 1, 1, f);
	if ((c1 & 0x40) == 0x40)
	    PACK[i] = ON;
	c1 &= 0x3F;
#if 0
	fwrite (&c1, 1, 1, out);
#endif

        xxi[i][0].fin = (int8) c1 << 4;

	/* volume */
	fread (&c1, 1, 1, f);
#if 0
	fwrite (&c1, 1, 1, out);
#endif

        xxi[i][0].vol = c1;

	/* loop start */
	fread (&c1, 1, 1, f);
	fread (&c2, 1, 1, f);

	if ((c1 == 0xFF) && (c2 == 0xFF)) {
	    c3 = 0x00;
	    c4 = 0x01;
#if 0
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c3, 1, 1, out);
	    fwrite (&c4, 1, 1, out);
#endif
            xxs[i].lps = 0;
            xxs[i].lpe = 2;

	    goto bla;
	}
#if 0
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
#endif
        xxs[i].lps = 2 * ((int)c1 << 8) | c2;

	l = j - ((c1 << 8) + c2);

	tmp = (uint8 *) & l;

#ifndef WORDS_BIGENDIAN
	/* WARNING !!! WORKS ONLY ON 80X86 computers ! */
	/* 68k machines code : c1 = *(tmp+2); */
	/* 68k machines code : c2 = *(tmp+3); */
	c1 = *(tmp + 1);
	c2 = *tmp;
#else
	c1 = *(tmp + 2);
	c2 = *(tmp + 3);
#endif

#if 0
	fwrite (&c1, 1, 1, out);
	fwrite (&c2, 1, 1, out);
#endif

        xxs[i].lpe = xxs[i].lps + 2 * (((int)c1 << 8) | c2);
        xxs[i].flg = xxs[i].lpe > xxs[i].lps ? WAVE_LOOPING : 0;
bla:
        xxi[i][0].pan = 0x80;
        xxi[i][0].sid = i;
        xxih[i].nsm = !!(xxs[i].len);
        xxih[i].rls = 0xfff;

        if (V (1) && xxs[i].len > 0) {
            report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
                i, xxs[i].len, xxs[i].lps, xxs[i].lpe,
                xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
                xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
        }
    }

#if 0
    /* go up to 31 samples */
    tmp = (uint8 *) malloc (30);
    bzero (tmp, 30);
    tmp[29] = 0x01;
    while (i != 31) {
	fwrite (tmp, 30, 1, out);
	i += 1;
    }
    free (tmp);
    /*printf ( "Whole sample size : %ld\n" , ssize ); */
#endif

    /* read tracks addresses per pattern */
    for (i = 0; i < PatMax; i++) {
	for (j = 0; j < 4; j++) {
	    fread (&c1, 1, 1, f);
	    fread (&c2, 1, 1, f);
	    taddr[i][j] = (c1 << 8) + c2;
	}
    }

    /* pattern table */
    for (PatPos = 0; PatPos < 128; PatPos++) {
	fread (&c1, 1, 1, f);
	if (c1 == 0xFF)
	    break;
	ptable[PatPos] = c1;    /* <--- /2 in p50a */
    }
   
    memcpy (xxo, ptable, 128);

#if 0
    /* write size of pattern list */
    fwrite (&PatPos, 1, 1, out);
#endif

    xxh->pat = PatMax;
    xxh->len = PatPos;
    xxh->trk = xxh->chn * xxh->pat;

#if 0
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
#endif

    tdata_addr = ftell (f);

    /* rewrite the track data */

    /*printf ( "sorting and depacking tracks data ... " ); */
    /*fflush ( stdout ); */
    for (i = 0; i < PatMax; i++) {
	Max = 63;
	for (j = 0; j < 4; j++) {
	    fseek (f, taddr[i][j] + tdata_addr, 0);
	    for (k = 0; k <= Max; k++) {
		fread (&c1, 1, 1, f);
		fread (&c2, 1, 1, f);
		fread (&c3, 1, 1, f);
		if (((c1 & 0x80) == 0x80) && (c1 != 0x80)) {
		    fread (&c4, 1, 1, f);
		    c1 = 0xFF - c1;
		    tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) | (ptk_table[c1 / 2][0]);
		    tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
		    c6 = c2 & 0x0f;
		    if (c6 == 0x08)
			c2 -= 0x08;
		    tdata[i * 4 + j][k * 4 + 2] = c2;
		    if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
			c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
		    tdata[i * 4 + j][k * 4 + 3] = c3;
		    if (c6 == 0x0D) {
			/* pattern break */
			Max = k;
			k = 9999l;
			continue;
		    }
		    if (c6 == 0x0B) {
			/* pattern jump */
			Max = k;
			k = 9999l;
			continue;
		    }

		    if (c4 < 0x80) {
			/* skip rows */
			k += c4;
			continue;
		    }
		    c4 = 0x100 - c4;
		    for (l = 0; l < c4; l++) {
			k += 1;
			tdata[i * 4 + j][k * 4] =
			    ((c1 << 4) & 0x10) | (ptk_table[c1 / 2][0]);
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
		if (c1 == 0x80) {
		    fread (&c4, 1, 1, f);
		    a = ftell (f);
		    c5 = c2;
		    fseek (f, -((c3 << 8) + c4), 1);
		    for (l = 0; l <= c5; l++, k++) {
			fread (&c1, 1, 1, f);
			fread (&c2, 1, 1, f);
			fread (&c3, 1, 1, f);
			if (((c1 & 0x80) == 0x80) && (c1 != 0x80)) {
			    fread (&c4, 1, 1, f);
			    c1 = 0xFF - c1;
			    tdata[i * 4 + j][k * 4] =
				((c1 << 4) & 0x10) | (ptk_table[c1 / 2] [0]);
			    tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
			    c6 = c2 & 0x0f;
			    if (c6 == 0x08)
				c2 -= 0x08;
			    tdata[i * 4 + j][k * 4 + 2] = c2;
			    if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
				c3 = (c3 > 0x7f) ? ( (0x100 - c3) << 4) : c3;
			    tdata[i * 4 + j][k * 4 + 3] = c3;
			    if (c6 == 0x0D) {
				Max = k;
				k = l = 9999l;
				/* pattern break */
				continue;
			    }
			    if (c6 == 0x0B) {
				Max = k;
				k = l = 9999l;
				/* pattern jump */
				continue;
			    }

			    if (c4 < 0x80) {
				/* skip rows */
				/*l += c4; */
				k += c4;
				continue;
			    }
			    c4 = 0x100 - c4;
			    /*l += (c4-1); */
			    for (b = 0; b < c4; b++) {
				k += 1;
				tdata[i * 4 + j][k * 4] = ((c1 << 4) &
				    0x10) | (ptk_table [c1 / 2] [0]);
				tdata[i * 4 + j][k * 4 + 1] = ptk_table [c1 / 2][1];
				c6 = c2 & 0x0f;
				if (c6 == 0x08)
				    c2 -= 0x08;
				tdata[i * 4 + j][k * 4 + 2] = c2;
				if (c6 == 0x05 || c6 == 0x06 || c6 == 0x0a)
				    c3 = (c3 > 0x7f) ?  ( (0x100 - c3) << 4) : c3;
				tdata[i * 4 + j][k * 4 + 3] = c3;
			    }
			}
			tdata[i * 4 + j][k * 4] = ((c1 << 4) & 0x10) | (ptk_table[c1 / 2][0]);
			tdata[i * 4 + j][k * 4 + 1] = ptk_table[c1 / 2][1];
			c6 = c2 & 0x0f;
			if (c6 == 0x08)
			    c2 -= 0x08;
			tdata[i * 4 + j][k * 4 + 2] = c2;
			if ((c6 == 0x05) || (c6 == 0x06) || (c6 == 0x0a))
			    c3 = (c3 > 0x7f) ? ((0x100 - c3) << 4) : c3;
			tdata[i * 4 + j][k * 4 + 3] = c3;
		    }
		    fseek (f, a, 0);
		    k -= 1;
		    continue;
		}

		tdata[i * 4 + j][k * 4] =
		    ((c1 << 4) & 0x10) | (ptk_table[c1 / 2][0]);
		tdata[i * 4 + j][k * 4 + 1] =
		    ptk_table[c1 / 2][1];
		c6 = c2 & 0x0f;
		if (c6 == 0x08)
		    c2 -= 0x08;
		tdata[i * 4 + j][k * 4 + 2] = c2;
		if ((c6 == 0x05) || (c6 == 0x06)
		    || (c6 == 0x0a)) c3 =
			(c3 >
			0x7f) ? ((0x100 -
			  c3) << 4) : c3;
		tdata[i * 4 + j][k * 4 + 3] = c3;
		if (c6 == 0x0D) {
		    /* pattern break */
		    Max = k;
		    k = 9999l;
		    continue;
		}
		if (c6 == 0x0B) {
		    /* pattern jump */
		    Max = k;
		    k = 9999l;
		    continue;
		}

	    }
	}
    }

    PATTERN_INIT ();

    /* write pattern data */

    /* Load and convert patterns */
    if (V (0))
        report ("Stored patterns: %d ", xxh->pat);

    tmp = (uint8 *) malloc (1024);
    for (i = 0; i < PatMax; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	bzero (tmp, 1024);
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		tmp[j * 16 + k * 4] = tdata[k + i * 4][j * 4];
		tmp[j * 16 + k * 4 + 1] = tdata[k + i * 4][j * 4 + 1];
		tmp[j * 16 + k * 4 + 2] = tdata[k + i * 4][j * 4 + 2];
		tmp[j * 16 + k * 4 + 3] = tdata[k + i * 4][j * 4 + 3];
	    }
	}
	for (j = 0; j < (64 * 4); j++) {
	    event = &EVENT (i, j % 4, j / 4);
	    cvt_pt_event (event, &tmp[j * 4]);
	}
	/* fwrite (tmp, 1024, 1, out); */

        if (V (0))
            report (".");
    }
    free (tmp);

    xxh->flg |= XXM_FLG_MODRNG;

    /* go to sample data address */
    fseek (f, sdata_addr, 0);

    if (V (0))
        report ("\nStored samples : %d ", xxh->smp);

    /* read and write sample data */
    for (i = 0; i < nins; i++) {
	fseek (f, sdata_addr + saddr[i + 1], 0);

#if 0
	insDataWork = (signed char *) malloc (smp_size[i]);
	bzero (insDataWork, smp_size[i]);
	fread (insDataWork, smp_size[i], 1, f);
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
	/*fwrite (insDataWork, smp_size[i], 1, out);*/
#endif
	
        if (!xxs[i].len)
            continue;
        xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
            &xxs[xxi[i][0].sid], NULL);

	/*free (insDataWork);*/

        if (V (0))
            report (".");

    }

    if (V (0))
        report ("\n");


#if 0
    if (GLOBAL_DELTA == ON)
	pw_p60a.flags |= PW_DELTA;
#endif

    return 0;
}


static int test_p60a (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	/* FIXME: add PW_REQUEST_DATA */

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
	/*PW_REQUEST_DATA (s, start + k * 6 + 4 + m * 8);*/

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

	/*PW_REQUEST_DATA (s, start + j + 1);*/

	l += 1;
	for (n = (k * 6 + 4 + m * 8 + l); n < j; n++) {
		if ((data[start + n] & 0x80) == 0x00) {
			if (data[start + n] > 0x49)
				return -1;

			if ((((data[start + n] << 4) & 0x10) |
				((data[start + n + 1] >> 4) & 0x0F)) > k)
				return -1;
			n += 2;
		} else
			n += 3;
	}

	return 0;
}

