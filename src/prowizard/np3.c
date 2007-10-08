/*
 * NoisePacker_v3.c   Copyright (C) 1998 Asle / ReDoX
 *                    Modified by Claudio Matsuoka
 *
 * Converts NoisePacked MODs back to ptk
 * Last revision : 26/11/1999 by Sylvain "Asle" Chipaux
 *                 reduced to only one FREAD.
 *                 Speed-up and Binary smaller.
 *
 * $Id: np3.c,v 1.4 2007-10-08 16:51:25 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_np3 (uint8 *, int);
static int depack_np3 (FILE *, FILE *);

struct pw_format pw_np3 = {
	"NP3",
	"Noisepacker v3",
	0x00,
	test_np3,
	depack_np3
};


static int depack_np3 (FILE *in, FILE *out)
{
	uint8 *tmp, *buffer;
	uint8 c1, c2, c3, c4;
	uint8 npos;
	uint8 nins;
	uint8 ptable[128];
	uint8 pat_max = 0x00;
	int filesize = 0l;
	int OverallCpt = 0l;
	int Max_Add = 0;
	int ssize = 0;
	int tsize;
	int taddr[128][4];
	int Unknown1;
	int i = 0, j = 0, k, l = 0;
	int tdata;
	int saddr = 0;

	memset(ptable, 0, 128);
	memset(taddr, 0, 128 * 4 * 4);

	/* get input file size */
	fseek (in, 0, 2);
	filesize = ftell (in);
	fseek (in, 0, 0);

	/* read but once input file */
	buffer = (uint8 *) malloc (filesize);
	memset(buffer, 0, filesize);
	fread (buffer, filesize, 1, in);

	/* read number of sample */
	nins = ((buffer[0] << 4) & 0xf0) | ((buffer[1] >> 4) & 0x0f);

	/* write title */
	tmp = (uint8 *) malloc (20);
	memset(tmp, 0, 20);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	/* read size of pattern list */
	npos = buffer[3] / 2;
	/*printf ( "Size of pattern list : %d\n" , npos ); */

	/* read 2 unknown bytes which size seem to be of some use ... */
	Unknown1 = (buffer[4] << 8) + buffer[5];

	/* read track data size */
	tsize = (buffer[6] << 8) + buffer[7];
	/*printf ( "tsize : %ld\n" , tsize ); */
	/* read sample descriptions */
	tmp = (uint8 *) malloc (22);
	memset(tmp, 0, 22);
	OverallCpt = 8;
	for (i = 0; i < nins; i++) {
		/* sample name */
		fwrite (tmp, 22, 1, out);
		/* size */
		fwrite (&buffer[OverallCpt + 6], 2, 1, out);
		ssize += (((buffer[OverallCpt + 6] << 8) +
		     buffer[OverallCpt + 7]) * 2);
		/* write finetune */
		fwrite (&buffer[OverallCpt], 1, 1, out);
		/* write volume */
		fwrite (&buffer[OverallCpt + 1], 1, 1, out);
		/* write loop start */
		fwrite (&buffer[OverallCpt + 14], 2, 1, out);
		/* write loop size */
		fwrite (&buffer[OverallCpt + 12], 2, 1, out);
		OverallCpt += 16;
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* fill up to 31 samples */
	free (tmp);
	tmp = (uint8 *) malloc (30);
	memset(tmp, 0, 30);
	tmp[29] = 0x01;
	while (i != 31) {
		fwrite (tmp, 30, 1, out);
		i += 1;
	}
	free (tmp);

	/* write size of pattern list */
	fwrite (&npos, 1, 1, out);

	/* write noisetracker byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* bypass 2 bytes ... seems always the same as in $02 */
	/* & bypass 2 other bytes which meaning is beside me */
	OverallCpt += 4;

	/* read pattern table */
	pat_max = 0x00;
	for (i = 0; i < npos; i++) {
		ptable[i] = ((buffer[OverallCpt + (i * 2)] << 8) +
			buffer[OverallCpt + (i * 2) + 1]) / 8;
		/*printf ( "%d," , ptable[i] ); */
		if (ptable[i] > pat_max)
			pat_max = ptable[i];
	}
	OverallCpt += npos * 2;
	pat_max += 1;
	/*printf ( "Number of pattern : %d\n" , pat_max ); */

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

	/* read tracks addresses per pattern */
	/*printf ( "\nOverallCpt : %ld\n" , OverallCpt ); */
	for (i = 0; i < pat_max; i++) {
		taddr[i][0] = (buffer[OverallCpt + (i * 8)] << 8) +
			buffer[OverallCpt + (i * 8) + 1];
		/*printf ( "\n%ld - " , taddr[i][0] ); */
		if (taddr[i][0] > Max_Add)
			Max_Add = taddr[i][0];
		taddr[i][1] = (buffer[OverallCpt + (i * 8) + 2] << 8) +
			buffer[OverallCpt + (i * 8) + 3];
		/*printf ( "%ld - " , taddr[i][1] ); */
		if (taddr[i][1] > Max_Add)
			Max_Add = taddr[i][1];
		taddr[i][2] = (buffer[OverallCpt + (i * 8) + 4] << 8) +
			buffer[OverallCpt + (i * 8) + 5];
		/*printf ( "%ld - " , taddr[i][2] ); */
		if (taddr[i][2] > Max_Add)
			Max_Add = taddr[i][2];
		taddr[i][3] = (buffer[OverallCpt + (i * 8) + 6] << 8) +
			buffer[OverallCpt + (i * 8) + 7];
		/*printf ( "%ld" , taddr[i][3] ); */
		if (taddr[i][3] > Max_Add)
			Max_Add = taddr[i][3];
	}
	tdata = (OverallCpt + (pat_max * 8));
	/*printf ( "tdata : %ld\n" , tdata ); */

	/* the track data now ... */
	tmp = (uint8 *) malloc (1024);
	for (i = 0; i < pat_max; i++) {
		memset(tmp, 0, 1024);
		for (j = 0; j < 4; j++) {
			l = tdata + taddr[i][3 - j];
			for (k = 0; k < 64; k++) {
				c1 = buffer[l];
				l += 1;
				if (c1 >= 0x80) {
					k += ((0x100 - c1) - 1);
					continue;
				}
				c2 = buffer[l];
				l += 1;
				c3 = buffer[l];
				l += 1;

				tmp[k * 16 + j * 4] = (c1 << 4) & 0x10;
				c4 = (c1 & 0xFE) / 2;
				tmp[k * 16 + j * 4] |= ptk_table[c4][0];
				tmp[k * 16 + j * 4 + 1] = ptk_table[c4][1];
				if ((c2 & 0x0f) == 0x08)
					c2 &= 0xf0;
				if ((c2 & 0x0f) == 0x07) {
					c2 = (c2 & 0xf0) + 0x0A;
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x06) {
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x05) {
					if (c3 > 0x80)
						c3 = 0x100 - c3;
					else
						c3 = (c3 << 4) & 0xf0;
				}
				if ((c2 & 0x0f) == 0x0E) {
					c3 = 0x01;
				}
				if ((c2 & 0x0f) == 0x0B) {
					c3 += 0x04;
					c3 /= 2;
				}
				tmp[k * 16 + j * 4 + 2] = c2;
				tmp[k * 16 + j * 4 + 3] = c3;
				if ((c2 & 0x0f) == 0x0D)
					k = 100;	/* to leave the loop */
			}
			if (l > saddr)
				saddr = l;
		}
		fwrite (tmp, 1024, 1, out);
	}
	free (tmp);

	/* sample data */
	if (((saddr / 2) * 2) != saddr)
		saddr += 1;
	OverallCpt = saddr;
	/*printf ( "Starting address of sample data : %x\n" , ftell ( in ) ); */
	fwrite (&buffer[saddr], ssize, 1, out);

	return 0;
}


static int test_np3 (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	PW_REQUEST_DATA (s, 10);

	/* size of the pattern table */
	j = (data[start + 2] << 8) + data[start + 3];

	if (((j / 2) * 2) != j || j == 0)
		return -1;

	/* test nbr of samples */
	if ((data[start + 1] & 0x0f) != 0x0C)
		return -1;

	l = ((data[start] << 4) & 0xf0) | ((data[start + 1] >> 4) & 0x0f);

	if (l > 0x1F || l == 0)
		return -1;
	/* l is the number of samples */

	/* test volumes */
	for (k = 0; k < l; k++) {
		if (data[start + 9 + k * 16] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (k = 0; k < l; k++) {
		o = (data[start + k * 16 + 14] << 8) +
			data[start + k * 16 + 15];
		m = (data[start + k * 16 + 20] << 8) +
			data[start + k * 16 + 21];
		n = (data[start + k * 16 + 22] << 8) +
			data[start + k * 16 + 23];
		o *= 2;
		m *= 2;
		n *= 2;

		if (o > 0xFFFF || m > 0xFFFF || n > 0xFFFF)
			return -1;

		if ((m + n) > (o + 2))
			return -1;

		if (n != 0 && m == 0)
			return -1;

		ssize += o;
	}

	if (ssize <= 4)
		return -1;

	/* small shit to gain some vars */
	l *= 16;
	l += 8;
	l += 4;
	/* l is the size of the header 'til the end of sample descriptions */

	/* test pattern table */
	n = 0;
	for (k = 0; k < j; k += 2) {
		m = ((data[start + l + k] << 8) + data[start + l + k + 1]);
		if (((m / 8) * 8) != m)
			return -1;
		if (m > n)
			n = m;
	}
	l += j;
	l += n;
	l += 8;		/* paske on a que l'address du dernier pattern .. */
	/* l is now the size of the header 'til the end of the track list */
	/* j is now available for use :) */
	/* n is the highest pattern number (*8) */

	/* test track data size */
	k = (data[start + 6] << 8) + data[start + 7];
	if (k <= 63)
		return -1;

	PW_REQUEST_DATA (s, start + l + k);

	/* test notes */
	/* re-calculate the number of sample */
	/* k is the track data size */
	j = ((data[start] << 4) & 0xf0) | ((data[start + 1] >> 4) & 0x0f);
	for (m = 0; m < k; m++) {
		if ((data[start + l + m] & 0x80) == 0x80)
			continue;

		/* si note trop grande et si effet = A */
		if ((data[start + l + m] > 0x49) ||
			((data[start + l + m + 1] & 0x0f) == 0x0A))
			return -1;

		/* si effet D et arg > 0x40 */
		if (((data[start + l + m + 1] & 0x0f) == 0x0D)
			&& (data[start + l + m + 2] > 0x40))
			return -1;

		/* sample nbr > ce qui est defini au debut ? */
		if ((((data[start + l + m] << 4) & 0x10) |
			((data[start + l + m + 1] >> 4) & 0x0f)) > j)
			return -1;

		/* all is empty ?!? ... cannot be ! */
		if (data[start + l + m] == 0 && data[start + l + m + 1] == 0 &&
			data[start + l + m + 2] == 0 && m < (k - 3))
			return -1;

		m += 2;
	}

	/* ssize is the size of the sample data */
	/* l is the size of the header 'til the track datas */
	/* k is the size of the track datas */

	return 0;
}
