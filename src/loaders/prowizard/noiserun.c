/*
 *  NoiseRunner.c	Copyright (C) 1997 Asle / ReDoX
 *			Copyright (C) 2010 Claudio Matsuoka
 *
 *  Converts NoiseRunner packed MODs back to Protracker
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int test_nru (uint8 *, int);
static int depack_nru (FILE *, FILE *);


struct pw_format pw_nru = {
	"NRU",
	"NoiseRunner",
	0x00,
	test_nru,
	depack_nru
};


static int fine_table[] = {
	0x0000, 0xffb8, 0xff70, 0xff28, 0xfee0, 0xfe98, 0xfe50, 0xfe08,
	0xfdc0, 0xfd78, 0xfd30, 0xfce8, 0xfca0, 0xfc58, 0xfc10, 0xfbc8
};


static int depack_nru(FILE *in, FILE *out)
{
	uint8 tmp[1025];
	uint8 ptable[128];
	uint8 note, ins, fxt, fxp;
	uint8 pat_data[1025];
	int fine;
	int i, j;
	int max_pat;
	long ssize = 0;

	pw_write_zero(out, 20);			/* title */

	for (i = 0; i < 31; i++) {		/* 31 samples */
		int vol, size, addr, start, lsize;

		pw_write_zero(out, 22);		/* sample name */
		read8(in);			/* bypass 0x00 */
		vol = read8(in);		/* read volume */
		addr = read32b(in);		/* read sample address */
		write16b(out, size = read16b(in)); /* read/write sample size */
		ssize += size * 2;
		start = read32b(in);		/* read loop start address */

		lsize = read16b(in);		/* read loop size */
		fine = read16b(in);		/* read finetune ?!? */

		for (j = 0; j < 16; j++) {
			if (fine == fine_table[j]) {
				fine = j;
				break;
			}
		}
		if (j == 16)
			fine = 0;

		write8(out, fine);		/* write fine */
		write8(out, vol);		/* write vol */
		write16b(out, (start - addr) / 2);	/* write loop start */
		write16b(out, lsize);		/* write loop size */
	}

	fseek(in, 950, SEEK_SET);
	write8(out, read8(in));			/* size of pattern list */
	write8(out, read8(in));			/* ntk byte */

	/* pattern table */
	max_pat = 0;
	fread(ptable, 128, 1, in);
	fwrite(ptable, 128, 1, out);
	for (i = 0; i < 128; i++) {
		if (ptable[i] > max_pat)
			max_pat = ptable[i];
	}
	max_pat++;

	write32b(out, PW_MOD_MAGIC);

	/* pattern data */
	fseek (in, 0x043c, SEEK_SET);
	for (i = 0; i < max_pat; i++) {
		memset(pat_data, 0, 1025);
		fread(tmp, 1024, 1, in);
		for (j = 0; j < 256; j++) {
			ins = (tmp[j * 4 + 3] >> 3) & 0x1f;
			note = tmp[j * 4 + 2];
			fxt = tmp[j * 4];
			fxp = tmp[j * 4 + 1];
			switch (fxt) {
			case 0x00:	/* tone portamento */
				fxt = 0x03;
				break;
			case 0x0C:	/* no fxt */
				fxt = 0x00;
				break;
			default:
				fxt >>= 2;
				break;
			}
			pat_data[j * 4] = ins & 0xf0;
			pat_data[j * 4] |= ptk_table[note / 2][0];
			pat_data[j * 4 + 1] = ptk_table[note / 2][1];
			pat_data[j * 4 + 2] = (ins << 4) & 0xf0;
			pat_data[j * 4 + 2] |= fxt;
			pat_data[j * 4 + 3] = fxp;
		}
		fwrite (pat_data, 1024, 1, out);
	}

	pw_move_data(out, in, ssize);		/* sample data */

	return 0;
}


static int test_nru(uint8 *data, int s)
{
	int i, j, k, l;
	int start = 0;
	int ssize;

	PW_REQUEST_DATA(s, 1500);

#if 0
	/* test 1 */
	if (i < 1080) {
		return -1;
	}
#endif

	if (readmem32b(data + start + 1080) != PW_MOD_MAGIC)
		return -1;

	/* test 2 */
	ssize = 0;
	for (i = 0; i < 31; i++)
		ssize += 2 * readmem16b(data + start + 6 + i * 16);
	if (ssize == 0)
		return -1;

	/* test #3 volumes */
	for (i = 0; i < 31; i++) {
		if (data[start + 1 + i * 16] > 0x40)
			return -1;
	}

	/* test #4  pattern list size */
	l = data[start + 950];
	if (l > 127 || l == 0) {
		return -1;
	}

	/* l holds the size of the pattern list */
	k = 0;
	for (j = 0; j < l; j++) {
		if (data[start + 952 + j] > k)
			k = data[start + 952 + j];
		if (data[start + 952 + j] > 127) {
			return -1;
		}
	}
	/* k holds the highest pattern number */
	/* test last patterns of the pattern list = 0 ? */
	while (j != 128) {
		if (data[start + 952 + j] != 0) {
			return -1;
		}
		j++;
	}
	/* k is the number of pattern in the file (-1) */
	k += 1;


	/* test #5 pattern data ... */
	for (j = 0; j < (k << 8); j++) {
		/* note > 48h ? */
		if (data[start + 1086 + j * 4] > 0x48) {
			return -1;
		}
		l = data[start + 1087 + j * 4];
		if (l & 0x07) {
			return -1;
		}
		l = data[start + 1084 + j * 4];
		if (l & 0x03) {
			return -1;
		}
	}

	return 0;
}
