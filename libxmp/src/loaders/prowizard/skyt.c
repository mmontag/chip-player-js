/* ProWizard
 * Copyright (C) 1997 Asle / ReDoX
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
 * Skyt_Packer.c
 */

#include "prowiz.h"


static int depack_skyt(HIO_HANDLE *in, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 ptable[128];
	uint8 pat_pos;
	uint8 pat[1024];
	int i = 0, j = 0, k = 0;
	int trkval[128][4];
	int trk_addr;
	int max_trk;
	int size, ssize = 0;

	memset(ptable, 0, sizeof(ptable));
	memset(trkval, 0, sizeof(trkval));

	pw_write_zero(out, 20);			/* write title */

	/* read and write sample descriptions */
	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);			/*sample name */
		write16b(out, size = hio_read16b(in));	/* sample size */
		ssize += size * 2;
		write8(out, hio_read8(in));			/* finetune */
		write8(out, hio_read8(in));			/* volume */
		write16b(out, hio_read16b(in));		/* loop start */
		write16b(out, hio_read16b(in));		/* loop size */
	}

	hio_read32b(in);			/* bypass 8 empty bytes */
	hio_read32b(in);
	hio_read32b(in);			/* bypass "SKYT" ID */

	pat_pos = hio_read8(in) + 1;		/* pattern table lenght */
	if (pat_pos >= 128) {
		return -1;
	}
	write8(out, pat_pos);
	write8(out, 0x7f);			/* write NoiseTracker byte */

	/* read track numbers ... and deduce pattern list */
	max_trk = 0;
	for (i = 0; i < pat_pos; i++) {
		for (j = 0; j < 4; j++) {
			trkval[i][j] = hio_read16b(in);
			if (trkval[i][j] > max_trk) {
					max_trk = trkval[i][j];
			}
		}
	}

	/* write pseudo pattern list */
	for (i = 0; i < 128; i++) {
		write8(out, i < pat_pos ? i : 0);
	}

	write32b(out, PW_MOD_MAGIC);		/* write ptk's ID */

	hio_read8(in);				/* bypass $00 unknown byte */

	/* get track address */
	trk_addr = hio_tell(in);

	/* track data */
	for (i = 0; i < pat_pos; i++) {
		memset(pat, 0, sizeof(pat));
		for (j = 0; j < 4; j++) {
			/* track 0 is blank and doesn't exist in the file. */
			if (trkval[i][j] == 0) {
				continue;
			}
			hio_seek(in, trk_addr + ((trkval[i][j] - 1)<<8), SEEK_SET);
			for (k = 0; k < 64; k++) {
				int x = k * 16 + j * 4;

				c1 = hio_read8(in);
				c2 = hio_read8(in);
				c3 = hio_read8(in);
				c4 = hio_read8(in);

				if (hio_error(in) || !PTK_IS_VALID_NOTE(c1)) {
					return -1;
				}

				pat[x] = (c2 & 0xf0) | ptk_table[c1][0];
				pat[x + 1] = ptk_table[c1][1];
				pat[x + 2] = ((c2 << 4) & 0xf0) | c3;
				pat[x + 3] = c4;
			}
		}
		fwrite(pat, 1024, 1, out);
	}

	/* skip to the end of the tracks/the start of the sample data. */
	if (hio_seek(in, trk_addr + (max_trk << 8), SEEK_SET) < 0) {
		return -1;
	}

	/* sample data */
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_skyt(const uint8 *data, char *t, int s)
{
	int i;

	PW_REQUEST_DATA(s, 8 * 31 + 12);

	/* test 2 */
	for (i = 0; i < 31; i++) {
		if (data[8 * i + 4] > 0x40)
			return -1;
	}

	if (readmem32b(data + 256) != MAGIC4('S','K','Y','T'))
		return -1;

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_skyt = {
	"SKYT Packer",
	test_skyt,
	depack_skyt
};

