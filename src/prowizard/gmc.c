/*
 * gmc.c    Copyright (C) 1997 Sylvain "Asle" Chipaux
 *          Modified by Claudio Matsuoka
 *
 * Depacks musics in the Game Music Creator format and saves in ptk.
 *
 * $Id: gmc.c,v 1.2 2006-02-13 12:50:34 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int depack_GMC(FILE *, FILE *);
static int test_GMC(uint8 *, int);

struct pw_format pw_gmc = {
	"GMC",
	"Game Music Creator",
	0x00,
	test_GMC,
	NULL,
	depack_GMC
};

static int depack_GMC(FILE * in, FILE * out)
{
	uint8 *tmp;
	uint8 c1 = 0x00, c2 = 0x00, c3 = 0x00, c4 = 0x00;
	uint8 ptable[128];
	uint8 Max = 0x00;
	uint8 PatPos;
	long ssize = 0;
	long i = 0, j = 0;
	// FILE *in,*out;

	bzero(ptable, 128);

	// in = fdopen (fd_in, "rb");
	// sprintf ( Depacked_OutName , "%ld.mod" , Cpt_Filename-1 );
	// out = fdopen (fd_out, "w+b");

	/* title */
	tmp = (uint8 *) malloc(20);
	bzero(tmp, 20);
	fwrite(tmp, 20, 1, out);
	free(tmp);

	/* read and write whole header */
	/*printf ( "Converting sample headers ... " ); */
	tmp = (uint8 *) malloc(22);
	bzero(tmp, 22);
	for (i = 0; i < 15; i++) {
		/* write name */
		fwrite(tmp, 22, 1, out);
		/* bypass 4 address bytes */
		fseek(in, 4, 1);
		/* size */
		fread(&c1, 1, 1, in);
		fread(&c2, 1, 1, in);
		fwrite(&c1, 1, 1, out);
		fwrite(&c2, 1, 1, out);
		ssize += (((c1 << 8) + c2) * 2);
		/* finetune */
		fseek(in, 1, 1);
		c1 = 0x00;
		fwrite(&c1, 1, 1, out);
		/* volume */
		fread(&c1, 1, 1, in);
		fwrite(&c1, 1, 1, out);
		/* bypass 4 address bytes */
		fseek(in, 4, 1);
		/* loop size */
		fread(&c1, 1, 1, in);
		fread(&c2, 1, 1, in);
		/* loop start */
		fread(&c3, 1, 1, in);
		fread(&c4, 1, 1, in);
		c4 /= 2;
		if ((c3 / 2) * 2 != c3) {
			if (c4 < 0x80)
				c4 += 0x80;
			else {
				c4 -= 0x80;
				c3 += 0x01;
			}
		}
		c3 /= 2;
		fwrite(&c3, 1, 1, out);
		fwrite(&c4, 1, 1, out);
		c2 /= 2;
		if ((c1 / 2) * 2 != c1) {
			if (c2 < 0x80)
				c2 += 0x80;
			else {
				c2 -= 0x80;
				c1 += 0x01;
			}
		}
		c1 /= 2;
		if ((c1 == 0x00) && (c2 == 0x00))
			c2 = 0x01;
		fwrite(&c1, 1, 1, out);
		fwrite(&c2, 1, 1, out);
	}
	free(tmp);
	tmp = (uint8 *) malloc(30);
	bzero(tmp, 30);
	tmp[29] = 0x01;
	for (i = 0; i < 16; i++)
		fwrite(tmp, 30, 1, out);
	free(tmp);
	/*printf ( "ok\n" ); */

	/* pattern list size */
	fseek(in, 0xf3, 0);
	fread(&PatPos, 1, 1, in);
	fwrite(&PatPos, 1, 1, out);

	/* ntk byte */
	c1 = 0x7f;
	fwrite(&c1, 1, 1, out);

	/* read and write size of pattern list */
	/*printf ( "Creating the pattern table ... " ); */
	for (i = 0; i < 100; i++) {
		fread(&c1, 1, 1, in);
		fread(&c2, 1, 1, in);
		ptable[i] = ((c1 << 8) + c2) / 1024;
	}
	fwrite(ptable, 128, 1, out);

	/* get number of pattern */
	Max = 0x00;
	for (i = 0; i < 128; i++) {
		if (ptable[i] > Max)
			Max = ptable[i];
	}
	/*printf ( "ok\n" ); */

	/* write ID */
	c1 = 'M';
	c2 = '.';
	c3 = 'K';
	fwrite(&c1, 1, 1, out);
	fwrite(&c2, 1, 1, out);
	fwrite(&c3, 1, 1, out);
	fwrite(&c2, 1, 1, out);

	/* pattern data */
	/*printf ( "Converting pattern datas " ); */
	tmp = (uint8 *) malloc(1024);
	fseek(in, 444, SEEK_SET);
	for (i = 0; i <= Max; i++) {
		bzero(tmp, 1024);
		fread(tmp, 1024, 1, in);
		for (j = 0; j < 256; j++) {
			switch (tmp[(j * 4) + 2] & 0x0f) {
			case 3:	/* replace by C */
				tmp[(j * 4) + 2] += 0x09;
				break;
			case 4:	/* replace by D */
				tmp[(j * 4) + 2] += 0x09;
				break;
			case 5:	/* replace by B */
				tmp[(j * 4) + 2] += 0x06;
				break;
			case 6:	/* replace by E0 */
				tmp[(j * 4) + 2] += 0x08;
				break;
			case 7:	/* replace by E0 */
				tmp[(j * 4) + 2] += 0x07;
				break;
			case 8:	/* replace by F */
				tmp[(j * 4) + 2] += 0x07;
				break;
			default:
				break;
			}
		}
		fwrite(tmp, 1024, 1, out);
		/*printf ( "." ); */
		/*fflush ( stdout ); */
	}
	free(tmp);
	/*printf ( " ok\n" ); */
	/*fflush ( stdout ); */

	/* sample data */
	/*printf ( "Saving sample data ... " ); */
	tmp = (uint8 *) malloc(ssize);
	bzero(tmp, ssize);
	fread(tmp, ssize, 1, in);
	fwrite(tmp, ssize, 1, out);
	free(tmp);
	/*printf ( "ok\n" ); */
	/*fflush ( stdout ); */

#if 0
	/* crap */
	Crap("GMC:Game Music Creator", -1, -1, out);
#endif

	fflush(in);
	fflush(out);

	/* printf ("done\n"); */

	return 0;
}

static int test_GMC(uint8 * data, int s)
{
	int j, k, l, m, n, o;
	int start = 0;

	PW_REQUEST_DATA(s, 1024);

#if 0
	/* test #1 */
	if (i < 7) {
/*printf ( "#1\n" );*/
		return -1;
	}
	start = i - 7;
#endif

	/* samples descriptions */
	m = 0;
	j = 0;
	for (k = 0; k < 15; k++) {
		o = (data[start + 16 * k + 4] << 8) + data[start + 16 * k + 5];
		n = (data[start + 16 * k + 12] << 8) +
		    data[start + 16 * k + 13];
		o *= 2;
		/* volumes */
		if (data[start + 7 + (16 * k)] > 0x40) {
/*printf ( "#2\n" );*/
			return -1;
		}
		/* size */
		if (o > 0xFFFF) {
/*printf ( "#2,1 Start:%ld\n" , start );*/
			return -1;
		}
		if (n > o) {
/*printf ( "#2,2 Start:%ld\n" , start );*/
			return -1;
		}
		m += o;
		if (o != 0)
			j = k + 1;
	}
	if (m <= 4) {
/*printf ( "#2,3 Start:%ld\n" , start );*/
		return -1;
	}
	/* j is the highest not null sample */

	/* pattern table size */
	if ((data[start + 243] > 0x64) || (data[start + 243] == 0x00)) {
		return -1;
	}

	/* pattern order table */
	l = 0;
	for (n = 0; n < 100; n++) {
		k = ((data[start + 244 + n * 2] << 8) +
		     data[start + 245 + n * 2]);
		if (((k / 1024) * 1024) != k) {
/*printf ( "#4 Start:%ld (k:%ld)\n" , start , k);*/
			return -1;
		}
		l = ((k / 1024) > l) ? k / 1024 : l;
	}
	l += 1;
	/* l is the number of pattern */
	if ((l == 1) || (l > 0x64)) {
		return -1;
	}

	PW_REQUEST_DATA(s, 444 + k * 1024 + n * 4 + 3);

	/* test pattern data */
	o = data[start + 243];
	for (k = 0; k < l; k++) {
		for (n = 0; n < 256; n++) {
			if ((data[start + 444 + k * 1024 + n * 4] > 0x03)
			    || ((data[start + 444 +
				      k * 1024 + n * 4 + 2] & 0x0f) >= 0x90)) {
/*printf ( "#5,0 Start:%ld (k:%ld)\n" , start , k);*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0xf0) >> 4) > j) {
/*printf ( "#5,1 Start:%ld (j:%ld) (where:%ld) (value:%x)\n"
         , start , j , start+444+k*1024+n*4+2
         , ((data[start+444+k*1024+n*4+2]&0xf0)>>4) );*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0x0f) == 3)
			    && (data[start + 444 +
				     k * 1024 + n * 4 + 3] > 0x40)) {
/*printf ( "#5,2 Start:%ld (j:%ld)\n" , start , j);*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0x0f) == 4)
			    && (data[start + 444 +
				     k * 1024 + n * 4 + 3] > 0x63)) {
/*printf ( "#5,3 Start:%ld (j:%ld)\n" , start , j);*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0x0f) == 5)
			    && (data[start + 444 +
				     k * 1024 + n * 4 + 3] > o + 1)) {
/*printf ( "#5,4 Start:%ld (effect:5)(o:%ld)(4th note byte:%x)\n" , start , j , data[start+444+k*1024+n*4+3]);*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0x0f) == 6)
			    && (data[start + 444 +
				     k * 1024 + n * 4 + 3] >= 0x02)) {
/*printf ( "#5,5 Start:%ld (at:%ld)\n" , start , start+444+k*1024+n*4+3 );*/
				return -1;
			}
			if (((data[start + 444 + k * 1024 +
				   n * 4 + 2] & 0x0f) == 7)
			    && (data[start + 444 +
				     k * 1024 + n * 4 + 3] >= 0x02)) {
/*printf ( "#5,6 Start:%ld (at:%ld)\n" , start , start+444+k*1024+n*4+3 );*/
				return -1;
			}
		}
	}

	return 0;
}
