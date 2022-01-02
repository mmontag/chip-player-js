/* ProWizard
 * Copyright (C) 1997 Asle / ReDoX
 * Modified in 2016 by Claudio Matsuoka
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
 * ProPacker_v1.0
 *
 * Converts back to ptk ProPacker v1 MODs
 */

#include "prowiz.h"

static int depack_pp10(HIO_HANDLE *in, FILE *out)
{
	uint8 c1;
	uint8 trk_num[4][128];
	uint8 len;
	uint8 tmp[8];
	uint8 pdata[1024];
	int i, j, k;
	int ntrk, size, ssize = 0;

	memset(trk_num, 0, sizeof(trk_num));

	pw_write_zero(out, 20);				/* write title */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {

		if (hio_read(tmp, 1, 8, in) != 8) {
			return -1;
		}

		pw_write_zero(out, 22);			/* sample name */

		size = readmem16b(tmp);			/* size */
		ssize += size * 2;

		if (tmp[4] == 0 && tmp[5] == 0) {	/* loop size */
			tmp[5] = 1;
		}

		if (fwrite(tmp, 1, 8, out) != 8) {
			return -1;
		}
	}

	len = hio_read8(in);			/* pattern table lenght */
	write8(out, len);

	c1 = hio_read8(in);			/* Noisetracker byte */
	write8(out, c1);

	/* read track list and get highest track number */
	for (ntrk = j = 0; j < 4; j++) {
		for (i = 0; i < 128; i++) {
			trk_num[j][i] = hio_read8(in);
			if (trk_num[j][i] > ntrk) {
				ntrk = trk_num[j][i];
			}
		}
	}

	/* write pattern table "as is" ... */
	for (i = 0; i < len; i++) {
		write8(out, i);
	}
	pw_write_zero(out, 128 - i);
	write32b(out, PW_MOD_MAGIC);		/* ID string */

	/* track/pattern data */
	for (i = 0; i < len; i++) {
		memset(pdata, 0, sizeof(pdata));
		for (j = 0; j < 4; j++) {
			hio_seek(in, 762 + (trk_num[j][i] << 8), SEEK_SET);
			for (k = 0; k < 64; k++) {
				hio_read(pdata + k * 16 + j * 4, 1, 4, in);
			}
		}
		fwrite(pdata, 1024, 1, out);
	}

	/* now, lets put file pointer at the beginning of the sample datas */
	if (hio_seek(in, 762 + ((ntrk + 1) << 8), SEEK_SET) < 0) {
		return -1;
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_pp10(const uint8 *data, char *t, int s)
{
	int i;
	int ntrk, ssize;

	PW_REQUEST_DATA(s, 1024);

#if 0
	/* test #1 */
	if (i < 3) {
		Test = BAD;
		return;
	}
	start = i - 3;
#endif

	/* noisetracker byte */
	if (data[249] > 0x7f) {
		return -1;
	}

	/* test #2 */
	ssize = 0;
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int size = readmem16b(d) << 1;
		int start = readmem16b(d + 4) << 1;
		int lsize = readmem16b(d + 6) << 1;

		ssize += size;

		if (lsize == 0) {
			return -1;
		}
		if (start != 0 && lsize <= 2) {
			return -1;
		}
		if (start + lsize > size + 2) {
			return -1;
		}
#if 0
		if (start != 0 && lsize == 0) {
			return -1;
		}
#endif
		if (d[2] > 0x0f) {		/* finetune > 0x0f ? */
			return -1;
		}

		if (d[3] > 0x40) {		/* volume > 0x40 ? */
			return -1;
		}

		if (start > size) {		/* loop start > size ? */
			return -1;
		}

		if (size > 0xffff) {		/* size > 0xffff ? */
			return -1;
		}
	}

	if (ssize <= 2) {
		return -1;
	}

	/* test #3   about size of pattern list */
	if (data[248] == 0 || data[248] > 127) {
		return -1;
	}

	/* get the highest track value */
	for (ntrk = i = 0; i < 512; i++) {
		if (data[250 + i] > ntrk) {
			ntrk = data[250 + i];
		}
	}
	ntrk++;

	PW_REQUEST_DATA(s, 762 + ntrk * 256);

	for (i = 0; i < ntrk * 64; i++) {
		if (data[762 + i * 4] > 0x13) {
			return -1;
		}
	}

	return 0;
}

const struct pw_format pw_pp10 = {
	"ProPacker 1.0",
	test_pp10,
	depack_pp10
};

