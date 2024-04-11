/* ProWizard
 * Copyright (C) 1997 Asle / ReDoX
 * Modified in 2016 by Claudio Matsuoka
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
 * Promizer_0.1_Packer.c
 *
 * Converts back to ptk Promizer 0.1 packed MODs
 */

#include <math.h>
#include "prowiz.h"

static int depack_pm01(HIO_HANDLE *in, FILE *out)
{
	uint8 ptable[128];
	uint8 len;
	uint8 npat;
	uint8 tmp[1024];
	uint8 pdata[1024];
	uint8 fin[31];
	uint8 oldins[4];
	int i, j;
	int psize, size, ssize = 0;
	int pat_ofs[128];

	memset(ptable, 0, sizeof(ptable));
	memset(pat_ofs, 0, sizeof(pat_ofs));
	memset(fin, 0, sizeof(fin));
	memset(oldins, 0, sizeof(oldins));

	pw_write_zero(out, 20);			/* title */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {

		if (hio_read(tmp, 1, 8, in) != 8) {
			return -1;
		}

		pw_write_zero(out, 22);			/* sample name */

		size = readmem16b(tmp);			/* size */
		ssize += size * 2;

		fin[i] = tmp[2];

		if (tmp[4] == 0 && tmp[5] == 0) {	/* loop size */
			tmp[5] = 1;
		}

		if (fwrite(tmp, 1, 8, out) != 8) {
			return -1;
		}
	}

	len = hio_read16b(in) >> 2;		/* pattern table lenght */
	write8(out, len);
	write8(out, 0x7f);			/* write NoiseTracker byte */

	/* read pattern address list */
	for (i = 0; i < 128; i++) {
		pat_ofs[i] = hio_read32b(in);
	}

	/* deduce pattern list and write it */
	for (npat = i = 0; i < 128; i++) {
		ptable[i] = pat_ofs[i] / 1024;
		write8(out, ptable[i]);
		if (ptable[i] > npat) {
			npat = ptable[i];
		}
	}
	npat++;

	write32b(out, PW_MOD_MAGIC);		/* ID string */

	psize = hio_read32b(in);		/* get pattern data size */

	if (npat * 1024 != psize) {
		return -1;
	}

	/* read and XOR pattern data */
	for (i = 0; i < npat; i++) {
		memset(pdata, 0, sizeof(pdata));
		if (hio_read(pdata, 1, 1024, in) != 1024) {
			return -1;
		}

		for (j = 0; j < 1024; j++) {
			if (j % 4 == 3) {
				pdata[j] = (240 - (pdata[j] & 0xf0)) +
						(pdata[j] & 0x0f);
				continue;
			}
			pdata[j] = pdata[j] ^ 0xff;
		}

		/* now take care of these 'finetuned' values ... pfff */
		oldins[0] = oldins[1] = oldins[2] = oldins[3] = 0x1f;
		for (j = 0; j < 64 * 4; j++) {
			uint8 *p = pdata + j * 4;
			int note = readmem16b(p) & 0x0fff;
			int ins = (p[0] & 0xf0) | ((p[2] >> 4) & 0x0f);

			if (note == 0) {
				continue;
			}

			if (ins == 0) {
				ins = oldins[i % 4];
			} else {
				oldins[i % 4] = ins;
			}

			note = (int)((double)note *
					pow(2, -1.0 * fin[j % 4] / 12 / 8));
			if (note > 0) {
				note = period_to_note(note) - 48;
			}

			p[0] = ptk_table[note][0];
			p[1] = ptk_table[note][1];
		}

		if (fwrite(pdata, 1, 1024, out) != 1024) {
			return -1;
		}
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}


static int test_pm01(const uint8 *data, char *t, int s)
{
	int i;
	int len, psize, ssize = 0;

	PW_REQUEST_DATA(s, 1024);

#if 0
	/* test #1 */
	if (i < 3) {
		Test = BAD;
		return;
	}
#endif

	/* test #2 */
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int size = readmem16b(data) << 1;
		int start = readmem16b(data + 4) << 1;
		int lsize = readmem16b(data + 6) << 1;

		ssize += size;

		if (d[2] > 0x0f) {		/* finetune > 0x0f ? */
			return -1;
		}

		/* loop start > size ? */
		if (start > size || lsize > size) {
			return -1;
		}

		if (lsize <= 2) {
			return -1;
		}
	}

	/* test #3   about size of pattern list */
	len = readmem16b(data + 248);
	if (len & 0x03) {
		return -1;
	}
	len >>= 2;

	if (len == 0 || len > 127) {
		return -1;
	}

	/* test #4  size of all the pattern data */
	/* k contains the size of the pattern list */
	psize = readmem32b(data + 762);
	if (psize < 1024 || psize > 131072) {
		return -1;
	}

	/* test #5  first pattern address != $00000000 ? */
	if (readmem32b(data + 250) != 0) {
		return -1;
	}

	/* test #6  pattern addresses */
	for (i = 0; i < len; i++) {
		int addr = readmem32b(data + 250 + i * 4);
		if (addr & 0x3ff || addr > 131072) {
			return -1;
		}
	}

	/* test #7  last patterns in pattern table != $00000000 ? */
	i += 4;		/* just to be sure */
	for (; i < 128; i++) {
		int addr = readmem32b(data + 250 + i * 4);
		if (addr != 0) {
			return -1;
		}
	}

	return 0;
}

const struct pw_format pw_pm01 = {
	"Promizer 0.1",
	test_pm01,
	depack_pm01
};

