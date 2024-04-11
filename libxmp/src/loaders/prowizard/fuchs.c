/* ProWizard
 * Copyright (C) 1999 Sylvain "Asle" Chipaux
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
 * FuchsTracker.c
 *
 * Depacks Fuchs Tracker modules
 */

#include "prowiz.h"


static int depack_fuchs(HIO_HANDLE *in, FILE *out)
{
	uint8 *tmp;
	uint8 max_pat;
	/*int ssize;*/
	uint8 data[1080];
	unsigned smp_len[16];
	unsigned loop_start[16];
	unsigned pat_size;
	unsigned i;

	memset(smp_len, 0, sizeof(smp_len));
	memset(loop_start, 0, sizeof(loop_start));
	memset(data, 0, sizeof(data));

	hio_read(data, 1, 10, in);		/* read/write title */
	/*ssize =*/ hio_read32b(in);		/* read all sample data size */

	/* read/write sample sizes */
	for (i = 0; i < 16; i++) {
		smp_len[i] = hio_read16b(in);
		data[42 + i * 30] = smp_len[i] >> 9;
		data[43 + i * 30] = smp_len[i] >> 1;
	}

	/* read/write volumes */
	for (i = 0; i < 16; i++) {
		data[45 + i * 30] = hio_read16b(in);
	}

	/* read/write loop start */
	for (i = 0; i < 16; i++) {
		loop_start[i] = hio_read16b(in);
		data[46 + i * 30] = loop_start[i] >> 1;
	}

	/* write replen */
	for (i = 0; i < 16; i++) {
		int loop_size;

		loop_size = smp_len[i] - loop_start[i];
		if (loop_size == 0 || loop_start[i] == 0) {
			data[49 + i * 30] = 1;
		} else {
			data[48 + i * 30] = loop_size >> 9;
			data[49 + i * 30] = loop_size >> 1;
		}
	}

	/* fill replens up to 31st sample wiz $0001 */
	for (i = 16; i < 31; i++) {
		data[49 + i * 30] = 1;
	}

	/* that's it for the samples ! */
	/* now, the pattern list */

	/* read number of pattern to play */
	data[950] = hio_read16b(in);
	data[951] = 0x7f;

	/* read/write pattern list */
	for (max_pat = i = 0; i < 40; i++) {
		uint8 pat = hio_read16b(in);
		data[952 + i] = pat;
		if (pat > max_pat) {
			max_pat = pat;
		}
	}

	/* write ptk's ID */
	if (fwrite(data, 1, 1080, out) != 1080) {
		return -1;
	}
	write32b(out, PW_MOD_MAGIC);

	/* now, the pattern data */

	/* bypass the "SONG" ID */
	hio_read32b(in);

	/* read pattern data size */
	pat_size = hio_read32b(in);

	/* Sanity check */
	if (!pat_size || pat_size > 0x20000 || (pat_size & 0x3))
		return -1;

	/* read pattern data */
	tmp = (uint8 *)malloc(pat_size);
	if (hio_read(tmp, 1, pat_size, in) != pat_size) {
		free(tmp);
		return -1;
	}

	/* convert shits */
	for (i = 0; i < pat_size; i += 4) {
		/* convert fx C arg back to hex value */
		if ((tmp[i + 2] & 0x0f) == 0x0c) {
			int x = tmp[i + 3];
			tmp[i + 3] = 10 * (x >> 4) + (x & 0xf);
		}
	}

	/* write pattern data */
	fwrite(tmp, pat_size, 1, out);
	free(tmp);

	/* read/write sample data */
	hio_read32b(in);			/* bypass "INST" Id */
	for (i = 0; i < 16; i++) {
		if (smp_len[i] != 0)
			pw_move_data(out, in, smp_len[i]);
	}

	return 0;
}

static int test_fuchs (const uint8 *data, char *t, int s)
{
	int i;
	int ssize, hdr_ssize;

	PW_REQUEST_DATA(s, 196);

	if (readmem32b(data + 192) != 0x534f4e47)	/* SONG */
		return -1;

	/* all sample size */
	hdr_ssize = readmem32b(data + 10);

	if (hdr_ssize <= 2 || hdr_ssize >= 65535 * 16)
		return -1;

	/* samples descriptions */
	ssize = 0;
	for (i = 0; i < 16; i++) {
		const uint8 *d = data + i * 2;
		int len = readmem16b(d + 14);
		int start = readmem16b(d + 78);

		/* volumes */
		if (d[46] > 0x40)
			return -1;

		if (len < start)
			return -1;

		ssize += len;
	}

	if (ssize <= 2 || ssize > hdr_ssize)
		return -1;

	/* get highest pattern number in pattern list */
	/*max_pat = 0;*/
	for (i = 0; i < 40; i++) {
		int pat = data[i * 2 + 113];
		if (pat > 40)
			return -1;
		/*if (pat > max_pat)
			max_pat = pat;*/
	}

#if 0
	/* input file not long enough ? */
	max_pat++;
	max_pat *= 1024;
	PW_REQUEST_DATA (s, k + 200);
#endif

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_fchs = {
	"Fuchs Tracker",
	test_fuchs,
	depack_fuchs
};

