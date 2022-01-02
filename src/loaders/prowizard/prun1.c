/* ProWizard
 * Copyright (C) 1996 Asle / ReDoX
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
 * ProRunner1.c
 *
 * Converts MODs packed with Prorunner v1.0
 */

#include "prowiz.h"


static int depack_pru1 (HIO_HANDLE *in, FILE *out)
{
	uint8 header[2048];
	uint8 c1, c2, c3, c4;
	uint8 npat, max;
	uint8 ptable[128];
	int ssize = 0;
	int i, j;

	memset(header, 0, sizeof(header));
	memset(ptable, 0, sizeof(ptable));

	/* read and write whole header */
	hio_read(header, 950, 1, in);
	fwrite(header, 950, 1, out);

	/* get whole sample size */
	for (i = 0; i < 31; i++) {
		ssize += readmem16b(header + i * 30 + 42) * 2;
	}

	/* read and write size of pattern list */
	write8(out, npat = hio_read8(in));

	memset(header, 0, sizeof(header));

	/* read and write ntk byte and pattern list */
	hio_read(header, 129, 1, in);
	fwrite(header, 129, 1, out);

	/* write ID */
	write32b(out, PW_MOD_MAGIC);

	/* get number of pattern */
	max = 0;
	for (i = 1; i < 129; i++) {
		if (header[i] > max)
			max = header[i];
	}

	/* pattern data */
	hio_seek(in, 1084, SEEK_SET);
	for (i = 0; i <= max; i++) {
		for (j = 0; j < 256; j++) {
			header[0] = hio_read8(in);
			header[1] = hio_read8(in);
			header[2] = hio_read8(in);
			header[3] = hio_read8(in);

			/* Sanity check */
			if (!PTK_IS_VALID_NOTE(header[1])) {
				return -1;
			}

			c1 = header[0] & 0xf0;
			c3 = (header[0] & 0x0f) << 4;
			c3 |= header[2];
			c4 = header[3];
			c1 |= ptk_table[header[1]][0];
			c2 = ptk_table[header[1]][1];
			write8(out, c1);
			write8(out, c2);
			write8(out, c3);
			write8(out, c4);
		}
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_pru1(const uint8 *data, char *t, int s)
{
	PW_REQUEST_DATA(s, 1084);

	if (readmem32b(data + 1080) != 0x534e542e)	/* "SNT." */
		return -1;

	/* test 2 */
	if (data[951] != 0x7f)
		return -1;

	/* test 3 */
	if (data[950] > 0x7f)
		return -1;

	pw_read_title(data, t, 20);

	return 0;
}

const struct pw_format pw_pru1 = {
	"Prorunner 1.0",
	test_pru1,
	depack_pru1
};
