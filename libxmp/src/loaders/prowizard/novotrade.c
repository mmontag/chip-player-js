/* ProWizard
 * Copyright (C) 2007 Asle / ReDoX
 * Modified in 2009,2014 by Claudio Matsuoka
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
 * NovoTrade.c
 */

#include "prowiz.h"


static int depack_ntp(HIO_HANDLE *in, FILE *out)
{
	uint8 buf[1024];
	int i, j;
	int pat_addr[128];
	short body_addr, smp_addr, nins, len, npat;
	int size, ssize = 0;

	hio_read32b(in);				/* skip MODU */

	pw_move_data(out, in, 16);		/* title */
	write32b(out, 0);

	body_addr = hio_read16b(in) + 4;		/* get 'BODY' address */
	nins = hio_read16b(in);			/* number of samples */
	len = hio_read16b(in);			/* size of pattern list */
	npat = hio_read16b(in);			/* number of patterns stored */
	smp_addr = hio_read16b(in) + body_addr + 4;	/* get 'SAMP' address */

	/* Sanity check */
	if (npat > 128 || len > 128) {
		return -1;
	}

	memset(buf, 0, sizeof(buf));

	/* instruments */
	for (i = 0; i < nins; i++) {
		int x = hio_read8(in);		/* instrument number */

		if (x > 30) {
			hio_seek(in, 7, SEEK_CUR);
			continue;
		}

		x *= 30;

		buf[x + 25] = hio_read8(in);	/* volume */

		size = hio_read16b(in);		/* size */
		buf[x + 22] = size >> 8;
		buf[x + 23] = size & 0xff;
		ssize += size * 2;

		buf[x + 26] = hio_read8(in);	/* loop start */
		buf[x + 27] = hio_read8(in);

		buf[x + 28] = hio_read8(in);	/* loop size */
		buf[x + 29] = hio_read8(in);
	}
	fwrite(buf, 930, 1, out);

	write8(out, len);
	write8(out, 0x7f);

	/* pattern list */
	memset(buf, 0, 128);
	for (i = 0; i < len; i++) {
		int pat = hio_read16b(in);
		/* Sanity check */
		if (pat >= npat)
			return -1;

		buf[i] = pat;
	}
	fwrite(buf, 128, 1, out);

	/* pattern addresses now */
	/* Where is on it */
	memset(pat_addr, 0, sizeof(pat_addr));
	for (i = 0; i < npat; i++)
		pat_addr[i] = hio_read16b(in);

	write32b(out, PW_MOD_MAGIC);

	/* pattern data now ... *gee* */
	for (i = 0; i < npat; i++) {
		hio_seek(in, body_addr + 4 + pat_addr[i], SEEK_SET);
		memset(buf, 0, sizeof(buf));

		for (j = 0; j < 64; j++) {
			int x = hio_read16b(in);

			if (x & 0x0001)
				hio_read(buf + j * 16, 1, 4, in);
			if (x & 0x0002)
				hio_read(buf + j * 16 + 4, 1, 4, in);
			if (x & 0x0004)
				hio_read(buf + j * 16 + 8, 1, 4, in);
			if (x & 0x0008)
				hio_read(buf + j * 16 + 12, 1, 4, in);
		}
		fwrite(buf, 1024, 1, out);
	}

	/* samples */
	hio_seek(in, smp_addr, SEEK_SET);
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_ntp(const uint8 *data, char *t, int s)
{
	int j, k;

	PW_REQUEST_DATA(s, 64);
	if (readmem32b(data) != MAGIC4('M','O','D','U'))
		return -1;

	j = readmem16b(data + 20) + 4;		/* "BODY" tag */
	k = readmem16b(data + 28) + j + 4;	/* "SAMP" tag */

	PW_REQUEST_DATA(s, j + 4);
	if (readmem32b(data + j) != MAGIC4('B','O','D','Y'))
		return -1;

	PW_REQUEST_DATA(s, k + 4);
	if (readmem32b(data + k) != MAGIC4('S','A','M','P'))
		return -1;

	pw_read_title(data + 4, t, 16);

	return 0;
}

const struct pw_format pw_ntp = {
	"Novotrade Packer",
	test_ntp,
	depack_ntp
};
