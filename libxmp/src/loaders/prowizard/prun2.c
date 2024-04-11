/* ProWizard
 * Copyright (C) 1996-1999 Asle / ReDoX
 * Modified in 2006,2007,2014 by Claudio Matsuoka
 * Modified in 2020 by Alice Rowan
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
 * ProRunner2.c
 *
 * Converts ProRunner v2 packed MODs back to Protracker
 */

#include "prowiz.h"


static int depack_pru2(HIO_HANDLE *in, FILE *out)
{
	uint8 header[2048];
	uint8 npat;
	uint8 ptable[128];
	uint8 max = 0;
	uint8 v[4][4];
	int size, ssize = 0;
	int i, j;

	memset(header, 0, sizeof(header));
	memset(ptable, 0, sizeof(ptable));
	memset(v, 0, sizeof(v));

	pw_write_zero(out, 20);				/* title */

	hio_seek(in, 8, SEEK_SET);

	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);			/*sample name */
		write16b(out, size = hio_read16b(in));	/* size */
		ssize += size * 2;
		write8(out, hio_read8(in));		/* finetune */
		write8(out, hio_read8(in));		/* volume */
		write16b(out, hio_read16b(in));		/* loop start */
		write16b(out, hio_read16b(in));		/* loop size */
	}

	write8(out, npat = hio_read8(in));		/* number of patterns */
	write8(out, hio_read8(in));			/* noisetracker byte */

	for (i = 0; i < 128; i++) {
		uint8 x;
		write8(out, x = hio_read8(in));
		max = (x > max) ? x : max;
	}

	write32b(out, PW_MOD_MAGIC);

	/* pattern data stuff */
	hio_seek(in, 770, SEEK_SET);

	for (i = 0; i <= max; i++) {
		for (j = 0; j < 256; j++) {
			uint8 c[4];
			memset(c, 0, sizeof(c));
			header[0] = hio_read8(in);
			if (header[0] == 0x80) {
				write32b(out, 0);
			} else if (header[0] == 0xc0) {
				fwrite(v[0], 4, 1, out);
				memcpy(c, v[0], 4);
			} else if (!PTK_IS_VALID_NOTE(header[0] >> 1)) {
				return -1;
			} else {
				header[1] = hio_read8(in);
				header[2] = hio_read8(in);

				c[0] = (header[1] & 0x80) >> 3;
				c[0] |= ptk_table[(header[0] >> 1)][0];
				c[1] = ptk_table[(header[0] >> 1)][1];
				c[2] = (header[1] & 0x70) << 1;
				c[2] |= (header[0] & 0x01) << 4;
				c[2] |= (header[1] & 0x0f);
				c[3] = header[2];

				fwrite(c, 1, 4, out);
			}

			/* rol previous values */
			memcpy(v[0], v[1], 4);
			memcpy(v[1], v[2], 4);
			memcpy(v[2], v[3], 4);

			memcpy(v[3], c, 4);
		}
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_pru2(const uint8 *data, char *t, int s)
{
	int k;

	PW_REQUEST_DATA(s, 12 + 31 * 8);

	if (readmem32b(data) != 0x534e5421)
		return -1;

#if 0
	/* check sample address */
	j = (data[i + 4] << 24) + (data[i + 5] << 16) + (data[i + 6] << 8) +
		data[i + 7];

	PW_REQUEST_DATA (s, j);
#endif

	/* test volumes */
	for (k = 0; k < 31; k++) {
		if (data[11 + k * 8] > 0x40)
			return -1;
	}

	/* test finetunes */
	for (k = 0; k < 31; k++) {
		if (data[10 + k * 8] > 0x0F)
			return -1;
	}

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_pru2 = {
	"Prorunner 2.0",
	test_pru2,
	depack_pru2
};
