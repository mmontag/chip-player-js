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
 * Module_Protector.c
 *
 * Converts MP packed MODs back to PTK MODs
 */

#include "prowiz.h"

#define MAGIC_TRK1	MAGIC4('T','R','K','1')


static int depack_mp(HIO_HANDLE *in, FILE *out)
{
	uint8 c1;
	uint8 ptable[128];
	uint8 max;
	int i;
	int size, ssize = 0;

	memset(ptable, 0, sizeof(ptable));

	pw_write_zero(out, 20);				/* title */

	if (hio_read32b(in) != MAGIC_TRK1)			/* TRK1 */
		hio_seek(in, -4, SEEK_CUR);

	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);			/* sample name */
		write16b(out, size = hio_read16b(in));	/* size */
		ssize += size * 2;
		write8(out, hio_read8(in));			/* finetune */
		write8(out, hio_read8(in));			/* volume */
		write16b(out, hio_read16b(in));		/* loop start */
		write16b(out, hio_read16b(in));		/* loop size */
	}

	write8(out, hio_read8(in));		/* pattern table length */
	write8(out, hio_read8(in));		/* NoiseTracker restart byte */

	for (max = i = 0; i < 128; i++) {
		write8(out, c1 = hio_read8(in));
		if (c1 > max)
			max = c1;
	}
	max++;

	write32b(out, PW_MOD_MAGIC);		/* M.K. */

	if (hio_read32b(in) != 0)			/* bypass unknown empty bytes */
		hio_seek(in, -4, SEEK_CUR);

	pw_move_data(out, in, 1024 * max);	/* pattern data */
	pw_move_data(out, in, ssize);		/* sample data */

	return 0;
}

static int test_mp_noid(const uint8 *data, char *t, int s)
{
	int i;
	int len, psize, hdr_ssize;

	PW_REQUEST_DATA(s, 378);

	/* test #2 */
	hdr_ssize = 0;
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int size = readmem16b(d) << 1;		/* size */
		int start = readmem16b(d + 4) << 1;	/* loop start */
		int lsize = readmem16b(d + 6) << 1;	/* loop size */

		hdr_ssize += size;

		/* finetune > 0x0f ? */
		if (d[2] > 0x0f)
			return -1;

		/* loop start+repsize > size ? */
		if (lsize != 2 && (start + lsize) > size)
			return -1;

		/* loop size > size ? */
		if (lsize > (size + 2))
			return -1;

		/* loop start != 0 and loop size = 0 */
		if (start != 0 && lsize <= 2)
			return -1;

		/* when size!=0  loopsize==0 ? */
		if (size != 0 && lsize == 0)
			return -1;
	}

	if (hdr_ssize <= 2)
		return -1;

	/* test #3 */
	len = data[248];
	if (len == 0 || len > 0x7f)
		return -1;

	/* test #4 */
	psize = 0;
	for (i = 0; i < 128; i++) {
		int pat = data[250 + i];
		if (pat > 0x7f)
			return -1;
		if (pat > psize)
			psize = pat;
		if (i > len + 3) {
			if (pat != 0)
				return -1;
		}
	}
	psize++;
	psize <<= 8;

	PW_REQUEST_DATA(s, 378 + psize * 4);

	/* test #5  ptk notes .. gosh ! (testing all patterns !) */
	for (i = 0; i < psize; i++) {
		const uint8 *d = data + 378 + i * 4;
		uint16 val;

		/* MadeInCroatia has l == 74 */
		if (*d > 19 && *d != 74)
			return -1;

		val = readmem16b(d) & 0x0fff;

		if (val > 0 && val < 0x71)
			return -1;
	}

	/* test #6  (loopStart+LoopSize > Sample ? ) */
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;

		int size = readmem16b(d) << 1;
		int lend = (readmem16b(d + 4) + readmem16b(d + 6)) << 1;

		if (lend > size + 2)
			return -1;
	}

	pw_read_title(NULL, t, 0);

	return 0;
}

static int test_mp_id(const uint8 *data, char *t, int s)
{
	int i;
	int len, psize;

	PW_REQUEST_DATA(s, 382);

	/* "TRK1" Module Protector */
	if (readmem32b(data) != MAGIC_TRK1)
		return -1;

	/* test #1 */
	for (i = 0; i < 31; i++) {
		if (data[6 + 8 * i] > 0x0f)
			return -1;
	}

	/* test #2 */
	len = data[252];
	if (len == 0 || len > 0x7f)
		return -1;

	/* test #4 */
	psize = 0;
	for (i = 0; i < 128; i++) {
		int pat = data[254 + i];
		if (pat > 0x7f)
			return -1;
		if (pat > psize)
			psize = pat;
	}
	psize++;
	psize <<= 8;

	PW_REQUEST_DATA(s, 382 + psize * 4);

	/* test #5  ptk notes .. gosh ! (testing all patterns !) */
	/* k contains the number of pattern saved */
	for (i = 0; i < psize; i++) {
		int x = data[382 + i * 4];
		if (x > 19)
			return -1;
	}

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_mp_id = {
	"Module Protector",
	test_mp_id,
	depack_mp
};

const struct pw_format pw_mp_noid = {
	"Module Protector noID",
	test_mp_noid,
	depack_mp
};
