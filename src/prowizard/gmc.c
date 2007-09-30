/*
 * gmc.c    Copyright (C) 1997 Sylvain "Asle" Chipaux
 *          Copyright (C) 2006-2007 Claudio Matsuoka
 *
 * Depacks musics in the Game Music Creator format and saves in ptk.
 *
 * $Id: gmc.c,v 1.6 2007-09-30 00:08:19 cmatsuoka Exp $
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
	depack_GMC
};

static int depack_GMC(FILE *in, FILE *out)
{
	uint8 *tmp;
	uint8 ptable[128];
	uint8 max;
	uint8 PatPos;
	uint16 len, looplen;
	long ssize = 0;
	long i = 0, j = 0;

	bzero(ptable, 128);

	/* title */
	for (i = 0; i < 20; i++)
		write8(out, 0);

	/* read and write whole header */
	/*printf ( "Converting sample headers ... " ); */
	for (i = 0; i < 15; i++) {
		for (j = 0; j < 22; j++)	/* write name */
			write8(out, 0);
		read32b(in);			/* bypass 4 address bytes */
		write16b(out, len = read16b(in));	/* size */
		ssize += len * 2;
		read8(in);
		write8(out, 0);			/* finetune */
		write8(out, read8(in));		/* volume */

		read32b(in);			/* bypass 4 address bytes */
#if 1
		/* loop size */
		looplen = read16b(in);
		write16b(out, looplen > 2 ? len - looplen : 0);
		write16b(out, looplen);

		read16b(in);

#else
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
#endif
	}

	tmp = (uint8 *)malloc(30);
	bzero(tmp, 30);
	tmp[29] = 0x01;
	for (i = 0; i < 16; i++)
		fwrite(tmp, 30, 1, out);
	free(tmp);

	fseek(in, 0xf3, 0);
	write8(out, PatPos = read8(in));	/* pattern list size */
	write8(out, 0x7f);			/* ntk byte */

	/* read and write size of pattern list */
	/*printf ( "Creating the pattern table ... " ); */
	for (i = 0; i < 100; i++)
		ptable[i] = read16b(in) / 1024;
	fwrite(ptable, 128, 1, out);

	/* get number of pattern */
	for (max = i = 0; i < 128; i++) {
		if (ptable[i] > max)
			max = ptable[i];
	}

	/* write ID */
	write32b(out, PW_MOD_MAGIC);

	/* pattern data */
	/*printf ( "Converting pattern datas " ); */
	tmp = (uint8 *) malloc(1024);
	fseek(in, 444, SEEK_SET);
	for (i = 0; i <= max; i++) {
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

	/* sample data */
	tmp = (uint8 *)malloc(ssize);
	bzero(tmp, ssize);
	fread(tmp, ssize, 1, in);
	fwrite(tmp, ssize, 1, out);
	free(tmp);

	return 0;
}

static int test_GMC(uint8 *data, int s)
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
/*printf ( "#4 Start:%d (k:%d)\n" , start , k);*/
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
			int offset = start + 444 + k * 1024 + n * 4;
			uint8 *d = &data[offset];

			if (offset > (PW_TEST_CHUNK - 4))
				return -1;
				
			/* First test fails with Jumping Jackson */
			if (/*(d[0] > 0x03) ||*/ ((d[2] & 0x0f) >= 0x90)) {
/*printf ( "#5,0 Start:%d (k:%d) %02x %02x\n" , start , k, d[0], d[2]);*/
				return -1;
			}
#if 0
			/* Test fails with Jumping Jackson */
			if (((d[2] & 0xf0) >> 4) > j) {
printf ( "#5,1 Start:%d (j:%d) (where:%d) (value:%x)\n"
         , start , j , offset + 2
         , ((data[offset + 2]&0xf0)>>4) );
				return -1;
			}
#endif
			if (((d[2] & 0x0f) == 3) && (d[3] > 0x40)) {
//printf ( "#5,2 Start:%d (j:%d)\n" , start , j);
				return -1;
			}
			if (((d[2] & 0x0f) == 4) && (d[3] > 0x63)) {
//printf ( "#5,3 Start:%d (j:%d)\n" , start , j);
				return -1;
			}
			if (((d[2] & 0x0f) == 5) && (d[3] > o + 1)) {
//printf ( "#5,4 Start:%d (effect:5)(o:%d)(4th note byte:%x)\n" , start , j , data[start+444+k*1024+n*4+3]);
				return -1;
			}
			if (((d[2] & 0x0f) == 6) && (d[3] >= 0x02)) {
//printf ( "#5,5 Start:%d (at:%d)\n" , start , start+444+k*1024+n*4+3 );
				return -1;
			}
			if (((d[2] & 0x0f) == 7) && (d[3] >= 0x02)) {
//printf ( "#5,6 Start:%d (at:%d)\n" , start , start+444+k*1024+n*4+3 );
				return -1;
			}
		}
	}

	return 0;
}
