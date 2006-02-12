/*
 * Zen_Packer.c   Copyright (C) 1998 Asle / ReDoX
 *                Modified by Claudio Matsuoka
 *
 * Converts ZEN packed MODs back to PTK MODs
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 *
 * $Id: zen.c,v 1.1 2006-02-12 22:04:43 cmatsuoka Exp $
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int test_zen (uint8 *, int);
static int depack_zen (uint8 *, FILE *);

struct pw_format pw_zen = {
	"ZEN",
	"Zen Packer",
	0x00,
	test_zen,
	depack_zen,
	NULL
};

static int depack_zen (uint8 *data, FILE *out)
{
	uint8 c1, c2, c3, c4, c5, c6;
	uint8 finetune, vol;
	uint8 pat_pos;
	uint8 pat_max;
	uint8 *tmp;
	uint8 note, ins, fxt, fxp;
	uint8 pat[1024];
	uint8 ptable[128];
	long ssize = 0;
	long paddr[128];
	long paddr_Real[128];
	long ptable_addr;
	long sdata_addr = 999999l;
	long i, j, k;
	long start = 0;
	long where = start;	/* main pointer to prevent fread() */

	bzero (paddr, 128 * 4);
	bzero (paddr_Real, 128 * 4);
	bzero (ptable, 128);

	/* read pattern table address */
	c1 = data[where++];
	c2 = data[where++];
	c3 = data[where++];
	c4 = data[where++];
	ptable_addr = (c1 << 24) + (c2 << 16) + (c3 << 8) + c4;

	/* read patmax */
	pat_max = data[where++];

	/* read size of pattern table */
	pat_pos = data[where++];
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* write title */
	tmp = (uint8 *) malloc (20);
	bzero (tmp, 20);
	fwrite (tmp, 20, 1, out);
	free (tmp);

	for (i = 0; i < 31; i++) {
		c1 = 0x00;
		fwrite (&c1, 1, 22, out);

		/* read finetune */
		c1 = data[where++];
		c2 = data[where++];
		finetune = ((c1 << 8) + c2) / 0x48;

		/* read volume */
		where += 1;
		vol = data[where++];

		/* read sample size */
		c1 = data[where++];
		c2 = data[where++];
		ssize += (((c1 << 8) + c2) * 2);
		fwrite (&c1, 1, 1, out);
		fwrite (&c2, 1, 1, out);

		/* write fine */
		fwrite (&finetune, 1, 1, out);

		/* write volume */
		fwrite (&vol, 1, 1, out);

		/* read loop size */
		c5 = data[where++];
		c6 = data[where++];

		/* read sample start address */
		c1 = data[where++];
		c2 = data[where++];
		c3 = data[where++];
		c4 = data[where++];
		k =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		if (k < sdata_addr)
			sdata_addr = k;

		/* read loop start address */
		c1 = data[where++];
		c2 = data[where++];
		c3 = data[where++];
		c4 = data[where++];
		j =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
		j -= k;
		j /= 2;

		/* write loop start */
		/* WARNING !!! WORKS ONLY ON 80X86 computers ! */
		/* 68k machines code : c3 = *(tmp+2); */
		/* 68k machines code : c4 = *(tmp+3); */
		tmp = (uint8 *) & j;
		c3 = *(tmp + 1);
		c4 = *tmp;
		fwrite (&c3, 1, 1, out);
		fwrite (&c4, 1, 1, out);

		/* write loop size */
		fwrite (&c5, 1, 1, out);
		fwrite (&c6, 1, 1, out);
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	/* write size of pattern list */
	fwrite (&pat_pos, 1, 1, out);

	/* write ntk byte */
	c1 = 0x7f;
	fwrite (&c1, 1, 1, out);

	/* read pattern table .. */
	where = start + ptable_addr;
	for (i = 0; i < pat_pos; i++) {
		c1 = data[where++];
		c2 = data[where++];
		c3 = data[where++];
		c4 = data[where++];
		paddr[i] =
			(c1 << 24) + (c2 << 16) +
			(c3 << 8) + c4;
	}

	/* deduce pattern list */
	c4 = 0x00;
	for (i = 0; i < pat_pos; i++) {
		if (i == 0) {
			ptable[0] = 0x00;
			paddr_Real[0] = paddr[0];
			c4 += 0x01;
			continue;
		}
		for (j = 0; j < i; j++) {
			if (paddr[i] == paddr[j]) {
				ptable[i] = ptable[j];
				break;
			}
		}
		if (j == i) {
			paddr_Real[c4] = paddr[i];
			ptable[i] = c4;
			c4 = c4 + 0x01;
		}
	}
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

	/* pattern data */
	/*printf ( "converting pattern datas " ); */
	for (i = 0; i <= pat_max; i++) {
		bzero (pat, 1024);
		where = start + paddr_Real[i];
/*fprintf ( info , "\n\npat %ld:\n" , i );*/
		for (j = 0; j < 256; j++) {
			c1 = data[where++];
			c2 = data[where++];
			c3 = data[where++];
			c4 = data[where++];
/*fprintf ( info , "%2d: %2x, %2x, %2x" , c1,c2,c3,c4 );*/

			note = (c2 & 0x7f) / 2;
			fxp = c4;
			ins = ((c2 << 4) & 0x10) | ((c3 >> 4) & 0x0f);
			fxt = c3 & 0x0f;

/*fprintf ( info , "<-- note:%-2x  smp:%-2x  fx:%-2x  fxval:%-2x\n"
               , note,ins,fxt,fxp );*/

			k = c1;
			pat[k * 4] = ins & 0xf0;
			pat[k * 4] |= ptk_table[note][0];
			pat[k * 4 + 1] = ptk_table[note][1];
			pat[k * 4 + 2] = fxt | ((ins << 4) & 0xf0);
			pat[k * 4 + 3] = fxp;
			j = c1;
		}
		fwrite (pat, 1024, 1, out);
		/*printf ( "." ); */
	}
	/*printf ( " ok\n" ); */

	/* sample data */
	fwrite (&data[start + sdata_addr],
		ssize, 1, out);

	return 0;
}


static int test_zen (uint8 *data, int s)
{
	int j, k, l, m, n, o;
	int start = 0, ssize;

	PW_REQUEST_DATA (s, 9 + 16 * 31);

	/* test #2 */
	l = ((data[start] << 24) + (data[start + 1] << 16) +
		(data[start + 2] << 8) + data[start + 3]);
	if (l < 502 || l > 2163190L)
		return -1;
	/* l is the address of the pattern list */

	for (k = 0; k < 31; k++) {
		/* volumes */
		if (data[start + 9 + (16 * k)] > 0x40)
			return -1;

		/* finetune */
		j = (data[start + 6 + (k * 16)] << 8) +
			data[start + 7 + (k * 16)];
		if (((j / 72) * 72) != j)
			return -1;
	}

	/* smp sizes .. */
	n = 0;
	for (k = 0; k < 31; k++) {
		o = (data[start + 10 + k * 16] << 8) +
			data[start + 11 + k * 16];
		m = (data[start + 12 + k * 16] << 8) +
			data[start + 13 + k * 16];
		j = ((data[start + 14 + k * 16] << 24) +
			(data[start + 15 + k * 16] << 16) +
			(data[start + 16 + k * 16] << 8) +
			data[start + 17 + k * 16]);
		o *= 2;
		m *= 2;

		/* sample size and loop size > 64k ? */
		if (o > 0xFFFF || m > 0xFFFF)
			return -1;

		/* sample address < pattern table address ? */
		if (j < l)
			return -1;

#if 0
		/* too big an address ? */
		if (j > in_size) {
			Test = BAD;
			return;
		}
#endif

		/* get the nbr of the highest sample address and its size */
		if (j > n) {
			n = j;
			ssize = o;
		}
	}
	/* n is the highest sample data address */
	/* ssize is the size of the same sample */

	/* test size of the pattern list */
	j = data[start + 5];
	if (j > 0x7f || j == 0)
		return -1;

	PW_REQUEST_DATA (s, start + l + j * 4 + 4);

	/* test if the end of pattern list is $FFFFFFFF */
	if ((data[start + l + j * 4] != 0xFF) ||
		(data[start + l + j * 4 + 1] != 0xFF) ||
		(data[start + l + j * 4 + 2] != 0xFF) ||
		(data[start + l + j * 4 + 3] != 0xFF))
		return -1;

	/* n is the highest address of a sample data */
	/* ssize is its size */

	return 0;
}
