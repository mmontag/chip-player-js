/*
 * Zen_Packer.c   Copyright (C) 1998 Asle / ReDoX
 *                Copyright (C) 2006-2007 Claudio Matsuoka
 *
 * Converts ZEN packed MODs back to PTK MODs
 ********************************************************
 * 13 april 1999 : Update
 *   - no more open() of input file ... so no more fread() !.
 *     It speeds-up the process quite a bit :).
 *
 * $Id: zen.c,v 1.4 2007-09-26 03:12:11 cmatsuoka Exp $
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

static int depack_zen(uint8 *data, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 finetune, vol;
	uint8 pat_pos;
	uint8 pat_max;
	uint8 note, ins, fxt, fxp;
	uint8 pat[1024];
	uint8 ptable[128];
	int size, ssize = 0;
	int paddr[128];
	int paddr_Real[128];
	int ptable_addr;
	int sdata_addr = 999999l;
	int i, j, k;
	int start = 0;
	int where = start;	/* main pointer to prevent fread() */

	bzero(paddr, 128 * 4);
	bzero(paddr_Real, 128 * 4);
	bzero(ptable, 128);

	/* read pattern table address */
	ptable_addr = readmem32b(data + where);
	where += 4;

	/* read patmax */
	pat_max = data[where++];

	/* read size of pattern table */
	pat_pos = data[where++];
	/*printf ( "Size of pattern list : %d\n" , pat_pos ); */

	/* write title */
	for (i = 0; i < 20; i++)
		write8(out, 0);

	for (i = 0; i < 31; i++) {
		for (j = 0; j < 22; j++)
			write8(out, 0);

		/* read finetune */
		finetune = readmem16b(data + where) / 0x48;
		where += 2;

		/* read volume */
		where += 1;
		vol = data[where++];

		/* read sample size */
		write16b(out, size = readmem16b(data + where));
		where += 2;
		ssize += size * 2;

		write8(out, finetune);	/* write fine */
		write8(out, vol);	/* write volume */

		/* read loop size */
		size = readmem16b(data + where);
		where += 2;

		/* read sample start address */
		k = readmem32b(data + where);
		where += 4;
		if (k < sdata_addr)
			sdata_addr = k;

		/* read loop start address */
		j = readmem32b(data + where);
		where += 4;
		j -= k;
		j /= 2;

		write16b(out, j);	/* write loop start */
		write16b(out, size);	/* write loop size */
	}
	/*printf ( "Whole sample size : %ld\n" , ssize ); */

	write8(out, pat_pos);	/* write size of pattern list */
	write8(out, 0x7f);	/* write ntk byte */

	/* read pattern table .. */
	where = start + ptable_addr;
	for (i = 0; i < pat_pos; i++) {
		paddr[i] = readmem32b(data + where);
		where += 4;
	}

	/* deduce pattern list */
	c4 = 0;
	for (i = 0; i < pat_pos; i++) {
		if (i == 0) {
			ptable[0] = 0;
			paddr_Real[0] = paddr[0];
			c4++;
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
			c4++;
		}
	}
	/*printf ( "Number of pattern : %d\n" , pat_max ); */

	fwrite(ptable, 128, 1, out);	/* write pattern table */
	write32b(out, PW_MOD_MAGIC);	/* write ptk's ID */

	/* pattern data */
	/*printf ( "converting pattern datas " ); */
	for (i = 0; i <= pat_max; i++) {
		bzero (pat, 1024);
		where = start + paddr_Real[i];
		for (j = 0; j < 256; j++) {
			c1 = data[where++];
			c2 = data[where++];
			c3 = data[where++];
			c4 = data[where++];

			note = (c2 & 0x7f) / 2;
			fxp = c4;
			ins = ((c2 << 4) & 0x10) | ((c3 >> 4) & 0x0f);
			fxt = c3 & 0x0f;

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
	fwrite(&data[start + sdata_addr], ssize, 1, out);

	return 0;
}


static int test_zen(uint8 *data, int s)
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
