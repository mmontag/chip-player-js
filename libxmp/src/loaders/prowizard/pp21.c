/* ProWizard
 * Copyright (C) 1997 Sylvain "Asle" Chipaux
 * Modified in 2006,2009,2014,2015 by Claudio Matsuoka
 * Modified in 2021 by Alice Rowan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * ProPacker_21.c
 *
 * Converts PP21 packed MODs back to PTK MODs
 * thanks to Gryzor and his ProWizard tool ! ... without it, this prog
 * would not exist !!!
 *
 * Modified in 2006,2009,2014 by Claudio Matsuoka
 * - Code cleanup
 *
 * Modified in 2015 by Claudio Matsuoka
 * - Add PP30 support
 */

#include "prowiz.h"


static int depack_pp21_pp30(HIO_HANDLE *in, FILE *out, int is_30)
{
	uint8 ptable[128];
	int max = 0;
	uint8 trk[4][128];
	int tptr[512][64];
	uint8 numpat;
	uint8 *tab;
	uint8 buf[1024];
	int i, j;
	int size;
	int ssize;
	int tabsize;		/* Reference Table Size */

	memset(ptable, 0, sizeof(ptable));
	memset(trk, 0, sizeof(trk));
	memset(tptr, 0, sizeof(tptr));

	pw_write_zero(out, 20);			/* title */

	ssize = 0;
	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);		/* sample name */
		write16b(out, size = hio_read16b(in));
		ssize += size * 2;
		write8(out, hio_read8(in));	/* finetune */
		write8(out, hio_read8(in));	/* volume */
		write16b(out, hio_read16b(in));	/* loop start */
		write16b(out, hio_read16b(in));	/* loop size */
	}

	numpat = hio_read8(in);			/* number of patterns */

	/* Sanity check */
	if (numpat > 128) {
		return -1;
	}

	write8(out, numpat);			/* number of patterns */
	write8(out, hio_read8(in));		/* NoiseTracker restart byte */

	max = 0;
	for (j = 0; j < 4; j++) {
		for (i = 0; i < 128; i++) {
			trk[j][i] = hio_read8(in);
			if (trk[j][i] > max)
				max = trk[j][i];
		}
	}

	/* write pattern table without any optimizing ! */
	for (i = 0; i < numpat; i++)
		write8(out, i);
	pw_write_zero(out, 128 - i);

	write32b(out, PW_MOD_MAGIC);		/* M.K. */

	/* PATTERN DATA code starts here */

	/*printf ("Highest track number : %d\n", max); */
	for (j = 0; j <= max; j++) {
		for (i = 0; i < 64; i++) {
			tptr[j][i] = hio_read16b(in);
			if (is_30) {
				tptr[j][i] >>= 2;
			}
		}
	}

	/* read "reference table" size */
	tabsize = hio_read32b(in);
	if (tabsize == 0) {
		return -1;
	}

	/* read "reference Table" */
	tab = (uint8 *)malloc(tabsize);
	if (hio_read(tab, tabsize, 1, in) != 1) {
		free(tab);
		return -1;
	}

	for (i = 0; i < numpat; i++) {
		memset(buf, 0, sizeof(buf));
		for (j = 0; j < 64; j++) {
			uint8 *b = buf + j * 16;
			memcpy(b, tab + tptr[trk[0][i]][j] * 4, 4);
			memcpy(b + 4, tab + tptr[trk[1][i]][j] * 4, 4);
			memcpy(b + 8, tab + tptr[trk[2][i]][j] * 4, 4);
			memcpy(b + 12, tab + tptr[trk[3][i]][j] * 4, 4);
		}
		fwrite (buf, 1024, 1, out);
	}

	free (tab);

	/* Now, it's sample data ... though, VERY quickly handled :) */
	pw_move_data(out, in, ssize);

	return 0;
}

static int depack_pp21(HIO_HANDLE *in, FILE *out)
{
	return depack_pp21_pp30(in, out, 0);
}

static int depack_pp30(HIO_HANDLE *in, FILE *out)
{
	return depack_pp21_pp30(in, out, 1);
}

static int test_pp21(const uint8 *data, char *t, int s)
{
	int i;
	int ssize, tsize, npat, max_ref;

	PW_REQUEST_DATA(s, 762);

	ssize = 0;
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int len = readmem16b(d) << 1;
		int start = readmem16b(d + 4) << 1;

		ssize += len;

		/* finetune > 0x0f ? */
		if (d[2] > 0x0f)
			return -1;

		/* volume > 0x40 ? */
		if (d[3] > 0x40)
			return -1;

		/* loop start > size ? */
		if (start > len)
			return -1;
	}

	if (ssize <= 2)
		return -1;

	/* test #3   about size of pattern list */
	npat = data[248];
	if (npat == 0 || npat > 127)
		return -1;

	/* get the highest track value */
	tsize = 0;
	for (i = 0; i < 512; i++) {
		int trk = data[250 + i];
		if (trk > tsize)
			tsize = trk;
	}

	tsize++;
	tsize <<= 6;

	PW_REQUEST_DATA(s, tsize * 2 + 4 + 762);

	/* test #4  track data value > $4000 ? */
	max_ref = 0;
	for (i = 0; i < tsize; i++) {
		int ref = readmem16b(data + i * 2 + 762);

		if (ref > 0x4000)
			return -1;

		if (ref > max_ref)
			max_ref = ref;

	}

	/* test #5  reference table size *4 ? */
	if (readmem32b(data + (tsize << 1) + 762) != (max_ref + 1) * 4)
		return -1;

	pw_read_title(NULL, t, 0);

	return 0;
}


static int test_pp30(const uint8 *data, char *t, int s)
{
	int i;
	int ssize, tsize, npat, max_ref, ref_size;

	PW_REQUEST_DATA(s, 762);

	ssize = 0;
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int len = readmem16b(d) << 1;
		int start = readmem16b(d + 4) << 1;

		ssize += len;

		/* finetune > 0x0f ? */
		if (d[2] > 0x0f)
			return -1;

		/* volume > 0x40 ? */
		if (d[3] > 0x40)
			return -1;

		/* loop start > size ? */
		if (start > len)
			return -1;
	}

	if (ssize <= 2)
		return -1;

	/* test #3   about size of pattern list */
	npat = data[248];
	if (npat == 0 || npat > 127)
		return -1;

	/* get the highest track value */
	tsize = 0;
	for (i = 0; i < 512; i++) {
		int trk = data[250 + i];
		if (trk > tsize)
			tsize = trk;
	}

	tsize++;
	tsize <<= 6;

	PW_REQUEST_DATA(s, (tsize * 2) + 4 + 762);

	/* test #4  track data value *4 ? */
	max_ref = 0;
	for (i = 0; i < tsize; i++) {
		int ref = readmem16b(data + i * 2 + 762);

		if (ref > max_ref)
			max_ref = ref;

		if (ref & 0x0003) {
			return -1;
		}
	}
	max_ref >>= 2;

	/* test #5  reference table size *4 ? */
	ref_size = readmem32b(data + (tsize << 1) + 762);

	if (ref_size > 0xffff) {
		return -1;
	}

	if (ref_size != ((max_ref + 1) << 2)) {
		return -1;
	}

	ref_size >>= 2;

	PW_REQUEST_DATA(s, (ref_size * 4) + (tsize * 2) + 4 + 762);

	/* test #6  data in reference table ... */
	for (i = 0; i < ref_size; i++) {
		const uint8 *d = data + (tsize * 2) + 766 + i * 4;
		uint8 fxt = d[2] & 0x0f;
		uint8 fxp = d[3];

		/* volume > 41 ? */
		if (fxt == 0x0c && fxp > 0x41) {
			return -1;
		}

		/* break > 64 (packed decimal) ? */
		if (fxt == 0x0d && (fxp > 0x64 || (fxp & 0xf) > 9)) {
			return -1;
		}

		/* jump > 128 */
		if (fxt == 0x0b && fxp > 0x7f) {
			return -1;
		}

		/* smp > 1f ? */
		if ((d[0] & 0xf0) > 0x10) {
			return -1;
		}
	}

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_pp21 = {
	"ProPacker 2.1",
	test_pp21,
	depack_pp21
};

const struct pw_format pw_pp30 = {
	"ProPacker 3.0",
	test_pp30,
	depack_pp30
};
