/*
 * NoisePacker_v1.c   Copyright (C) 1997 Asle / ReDoX
 *
 * Converts NoisePacked MODs back to ptk
 *
 * Modified in 2006,2007,2014 by Claudio Matsuoka
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"


static int depack_np1(FILE *in, FILE *out)
{
	uint8 tmp[1024];
	uint8 c1, c2, c3, c4;
	uint8 ptable[128];
	int len, nins, npat;
	int max_addr;
	int size, ssize = 0;
	/*int tsize;*/
	int trk_addr[128][4];
	int i, j, k;
	int trk_start;

	memset(ptable, 0, 128);
	memset(trk_addr, 0, 128 * 4 * 4);

	c1 = read8(in);				/* read number of samples */
	c2 = read8(in);
	nins = ((c1 << 4) & 0xf0) | ((c2 >> 4) & 0x0f);

	pw_write_zero(out, 20);			/* write title */

	len = read16b(in) / 2;			/* size of pattern list */
	read16b(in);				/* 2 unknown bytes */
	/*tsize =*/ read16b(in);		/* read track data size */

	/* read sample descriptions */
	for (i = 0; i < nins; i++) {
		read32b(in);			/* bypass 4 unknown bytes */
		pw_write_zero(out, 22);		/* sample name */
		write16b(out, size = read16b(in));	/* size */
		ssize += size * 2;
		write8(out, read8(in));		/* finetune */
		write8(out, read8(in));		/* volume */
		read32b(in);			/* bypass 4 unknown bytes */
		size = read16b(in);		/* read loop size */
		write16b(out, read16b(in) / 2);	/* loop start */
		write16b(out, size);		/* write loop size */
	}

	/* fill up to 31 samples */
	memset(tmp, 0, 30);
	tmp[29] = 0x01;
	for (; i < 31; i++)
		fwrite(tmp, 30, 1, out);

	write8(out, len);		/* write size of pattern list */
	write8(out, 0x7f);		/* write noisetracker byte */

	fseek(in, 2, SEEK_CUR);		/* always $02? */
	fseek(in, 2, SEEK_CUR);		/* unknown */

	/* read pattern table */
	npat = 0;
	for (i = 0; i < len; i++) {
		ptable[i] = read16b(in) >> 3;
		if (ptable[i] > npat)
			npat = ptable[i];
	}
	npat++;

	fwrite(ptable, 128, 1, out);	/* write pattern table */
	write32b(out, PW_MOD_MAGIC);	/* write ptk ID */

	/* read tracks addresses per pattern */
	max_addr = 0;
	for (i = 0; i < npat; i++) {
		if ((trk_addr[i][0] = read16b(in)) > max_addr)
			max_addr = trk_addr[i][0];
		if ((trk_addr[i][1] = read16b(in)) > max_addr)
			max_addr = trk_addr[i][1];
		if ((trk_addr[i][2] = read16b(in)) > max_addr)
			max_addr = trk_addr[i][2];
		if ((trk_addr[i][3] = read16b(in)) > max_addr)
			max_addr = trk_addr[i][3];
	}
	trk_start = ftell(in);

	/* the track data now ... */
	for (i = 0; i < npat; i++) {
		memset(tmp, 0, 1024);
		for (j = 0; j < 4; j++) {
			fseek(in, trk_start + trk_addr[i][3 - j], SEEK_SET);
			for (k = 0; k < 64; k++) {
				int x = k * 16 + j * 4;

				c1 = read8(in);
				c2 = read8(in);
				c3 = read8(in);
				c4 = (c1 & 0xfe) / 2;

				tmp[x] = ((c1 << 4) & 0x10) | ptk_table[c4][0];
				tmp[x + 1] = ptk_table[c4][1];

				switch (c2 & 0x0f) {
				case 0x08:
					c2 &= 0xf0;
					break;
				case 0x07:
					c2 = (c2 & 0xf0) + 0x0a;
					/* fall through */
				case 0x06:
				case 0x05:
					c3 = c3 > 0x80 ? 0x100 - c3 :
							(c3 << 4) & 0xf0;
					break;
				case 0x0b:
					c3 = (c3 + 4) / 2;
					break;
				}

				tmp[x + 2] = c2;
				tmp[x + 3] = c3;
			}
		}
		fwrite(tmp, 1024, 1, out);
	}

	/* sample data */
	fseek(in, max_addr + 192 + trk_start, SEEK_SET);
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_np1(uint8 *data, char *t, int s)
{
	int num_ins, ssize, hdr_size, ptab_size, trk_size, max_pptr;
	int i;

	/* size of the pattern table */
	ptab_size = readmem16b(data + 2);
	if (ptab_size == 0 || ptab_size & 1)
		return -1;

	/* test nbr of samples */
	if ((data[1] & 0x0f) != 0x0c)
		return -1;

	num_ins = ((data[0] << 4) & 0xf0) | ((data[1] >> 4) & 0x0f);
	if (num_ins == 0 || num_ins > 0x1f)
		return -1;

	PW_REQUEST_DATA(s, 15 + num_ins * 16);

	/* test volumes */
	for (i = 0; i < num_ins; i++) {
		if (data[15 + i * 16] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (i = 0; i < num_ins; i++) {
		uint8 *d = data + i * 16;

		int len = readmem16b(d + 12) << 1;
		int start = readmem16b(d + 20) << 1;
		int lsize = readmem16b(d + 22);

		if (len > 0xffff || start > 0xffff || lsize > 0xffff)
			return -1;

		if (start + lsize > len + 2)
			return -1;

		if (start == 0 && lsize != 0)
			return -1;

		ssize += len;
	}

	if (ssize <= 4)
		return -1;

	/* size of the header til the end of sample descriptions */
	hdr_size = num_ins * 16 + 8 + 4;

	/* test pattern table */
	max_pptr = 0;
	for (i = 0; i < ptab_size; i += 2) {
		int pptr = readmem16b(data + hdr_size + i);
		if (pptr % 8)
			return -1;
		if (pptr > max_pptr)
			max_pptr = pptr;
	}

	/* paske on a que l'address du dernier pattern .. */
	/* size of the header 'til the end of the track list */
	hdr_size += ptab_size + max_pptr + 8;

	/* test track data size */
	trk_size = readmem16b(data + 6);
	if (trk_size < 192 || (trk_size & 0x3f))
		return -1;

	PW_REQUEST_DATA(s, hdr_size + trk_size);

	/* test notes */
	for (i = 0; i < trk_size; i += 3) {
		if (data[hdr_size + i] > 0x49)
			return -1;
	}

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_np1 = {
	"NoisePacker v1",
	test_np1,
	depack_np1
};
