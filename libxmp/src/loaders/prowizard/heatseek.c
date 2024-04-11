/* ProWizard
 * Copyright (C) 1997 Asle / ReDoX
 * Modified in 2006,2007,2014 by Claudio Matsuoka
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
 * Heatseeker_mc1.0.c
 *
 * Converts back to ptk Heatseeker packed MODs
 *
 * Asle's note: There's a good job ! .. gosh !.
 */

#include "prowiz.h"


static int depack_crb(HIO_HANDLE *in, FILE *out)
{
	uint8 c1;
	uint8 ptable[128];
	uint8 pat_pos, pat_max;
	uint8 pat[1024];
	int taddr[512];
	int i, j, k, l, m;
	int size, ssize = 0;

	memset(ptable, 0, sizeof(ptable));
	memset(taddr, 0, sizeof(taddr));

	pw_write_zero(out, 20);				/* write title */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);			/*sample name */
		write16b(out, size = hio_read16b(in));	/* size */
		ssize += size * 2;
		write8(out, hio_read8(in));			/* finetune */
		write8(out, hio_read8(in));			/* volume */
		write16b(out, hio_read16b(in));		/* loop start */
		size = hio_read16b(in);			/* loop size */
		write16b(out, size ? size : 1);
	}

	write8(out, pat_pos = hio_read8(in));		/* pat table length */
	write8(out, hio_read8(in)); 			/* NoiseTracker byte */

	/* read and write pattern list and get highest patt number */
	for (pat_max = i = 0; i < 128; i++) {
		write8(out, c1 = hio_read8(in));
		if (c1 > pat_max)
			pat_max = c1;
	}
	pat_max++;

	/* write ptk's ID */
	write32b(out, PW_MOD_MAGIC);

	/* pattern data */
	for (i = 0; i < pat_max; i++) {
		memset(pat, 0, sizeof(pat));
		for (j = 0; j < 4; j++) {
			int x = hio_tell(in);
			if (x < 0) {
				return -1;
			}
			taddr[i * 4 + j] = x;
			for (k = 0; k < 64; k++) {
				int y = k * 16 + j * 4;

				c1 = hio_read8(in);
				if (c1 == 0x80) {
					k += hio_read24b(in);
					continue;
				}
				if (c1 == 0xc0) {
					m = hio_read24b(in);
					l = hio_tell(in);

					/* Sanity check */
					if (l < 0 || (unsigned int)m >= 2048U)
						return -1;

					hio_seek(in, taddr[m >> 2], SEEK_SET);
					for (m = 0; m < 64; m++) {
						x = m * 16 + j * 4;

						c1 = hio_read8(in);
						if (c1 == 0x80) {
							m += hio_read24b(in);
							continue;
						}
						pat[x] = c1;
						pat[x + 1] = hio_read8(in);
						pat[x + 2] = hio_read8(in);
						pat[x + 3] = hio_read8(in);
					}
					hio_seek(in, l, SEEK_SET);
					k += 100;
					continue;
				}
				pat[y] = c1;
				pat[y + 1] = hio_read8(in);
				pat[y + 2] = hio_read8(in);
				pat[y + 3] = hio_read8(in);
			}
		}
		fwrite (pat, 1024, 1, out);
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_crb(const uint8 *data, char *t, int s)
{
	int i, j, k;
	int ssize, max, idx, init_data;

	PW_REQUEST_DATA (s, 378);

	/* size of the pattern table */
	if (data[248] > 0x7f || data[248] == 0x00)
		return -1;

	/* test noisetracker byte */
	if (data[249] != 0x7f)
		return -1;

	/* test samples */
	ssize = 0;
	for (i = 0; i < 31; i++) {
		int len, start, lsize;
		const uint8 *d = data + i * 8;

		if (d[2] > 0x0f)
			return -1;

		/* test volumes */
		if (d[3] > 0x40)
			return -1;

		len = readmem16b(d) << 1;		/* size */
		start = readmem16b(d + 4) << 1;		/* loop start */
		lsize = readmem16b(d + 6) << 1;		/* loop size */

		if (len > 0xffff || start > 0xffff || lsize > 0xffff)
			return -1;

		if (lsize != 0 && lsize != 2 && (start + lsize) > len)
			return -1;

		if (start != 0 && lsize <= 2)
			return -1;

		ssize += len;
	}

	/* printf ("3\n"); */
	if (ssize <= 4)
		return -1;

	/* test pattern table */
	max = 0;
	for (i = 0; i < 128; i++) {
		if (data[250 + i] > 0x7f)
			return -1;
		if (data[250 + i] > max)
			max = data[250 + i];
	}
	max++;

	/* Request either the upper bound of the packed pattern data size
	 * or the sample data size, which is "known" to be valid. */
	init_data = MIN(4 * max * 4 * 64, ssize);
	PW_REQUEST_DATA(s, 378 + init_data);

	/* test notes */
	idx = 0;
	for (i = 0; i < max; i++) {
		for (j = 0; j < 4; j++) {
			for (k = 0; k < 64; k++) {
				const uint8 *d = data + 378 + idx;
				/* Slow... */
				if (idx >= init_data) {
					PW_REQUEST_DATA(s, 378 + idx + 4);
				}
				switch (d[0] & 0xC0) {
				case 0x00:
					if ((d[0] & 0x0F) > 0x03)
						return -1;
					idx += 4;
					break;
				case 0x80:
					if (d[1] != 0)
						return -1;
					k += d[3];
					idx += 4;
					break;
				case 0xC0:
					if (d[1] != 0)
						return -1;
					k = 100;
					idx += 4;
					break;
				default:
					break;
				}
			}
		}
	}

	/* k is the size of the pattern data */
	/* ssize is the size of the sample data */

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_crb = {
	"Heatseeker 1.0",
	test_crb,
	depack_crb
};
