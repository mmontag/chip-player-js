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
 * Promizer_10c.c
 *
 * Converts PM10c packed MODs back to PTK MODs
 */

#include "prowiz.h"


static int depack_p10c(HIO_HANDLE *in, FILE *out)
{
	uint8 c1, c2;
	int pat_max;
	int tmp1, tmp2;
	int refmax;
	int refsize;
	uint8 pnum[128];
	uint8 pnum1[128];
	int paddr[128];
	int paddr1[128];
	int paddr2[128];
	short pptr[64][256];
	int num_pat;
	uint8 *reftab;
	uint8 pat[128][1024];
	int i, j, k, l;
	int size, ssize;
	int psize;
	int smp_ofs;
	uint8 fin[31];
	uint8 oldins[4];

	memset(pnum, 0, sizeof(pnum));
	memset(pnum1, 0, sizeof(pnum1));
	memset(pptr, 0, sizeof(pptr));
	memset(pat, 0, sizeof(pat));
	memset(fin, 0, sizeof(fin));
	memset(oldins, 0, sizeof(oldins));
	memset(paddr, 0, sizeof(paddr));
	memset(paddr1, 0, sizeof(paddr1));

	for (i = 0; i < 128; i++)
		paddr2[i] = 9999L;

	pw_write_zero(out, 20);				/* title */

	/* bypass replaycode routine */
	hio_seek(in, 4460, SEEK_SET);

	ssize = 0;
	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);			/*sample name */
		write16b(out, size = hio_read16b(in));	/* size */
		ssize += size * 2;
		write8(out, fin[i] = hio_read8(in));	/* fin */
		write8(out, hio_read8(in));			/* volume */
		write16b(out, hio_read16b(in));		/* loop start */
		write16b(out, hio_read16b(in));		/* loop size */
	}

	num_pat = hio_read16b(in) / 4;			/* pat table length */

	/* Sanity check */
	if (num_pat > 128) {
		return -1;
	}

	write8(out, num_pat);
	write8(out, 0x7f);				/* NoiseTracker byte */

	for (i = 0; i < 128; i++)
		paddr[i] = hio_read32b(in);

	/* ordering of patterns addresses */

	pat_max = 0;
	for (i = 0; i < num_pat; i++) {
		if (i == 0) {
			pnum[0] = 0;
			continue;
		}

		for (j = 0; j < i; j++) {
			if (paddr[i] == paddr[j]) {
				pnum[i] = pnum[j];
				break;
			}
		}
		if (j == i)
			pnum[i] = (++pat_max);
	}

	/* correct re-order */
	for (i = 0; i < num_pat; i++)
		paddr1[i] = paddr[i];

restart:
	for (i = 0; i < num_pat; i++) {
		for (j = 0; j < i; j++) {
			if (paddr1[i] < paddr1[j]) {
				tmp2 = pnum[j];
				pnum[j] = pnum[i];
				pnum[i] = tmp2;
				tmp1 = paddr1[j];
				paddr1[j] = paddr1[i];
				paddr1[i] = tmp1;
				goto restart;
			}
		}
	}

	for (j = i = 0; i < num_pat; i++) {
		if (i == 0) {
			paddr2[j] = paddr1[i];
			continue;
		}

		if (paddr1[i] == paddr2[j])
			continue;
		paddr2[++j] = paddr1[i];
	}

	for (c1 = 0; c1 < num_pat; c1++) {
		for (c2 = 0; c2 < num_pat; c2++) {
			if (paddr[c1] == paddr2[c2])
				pnum1[c1] = c2;
		}
	}

	for (i = 0; i < num_pat; i++)
		pnum[i] = pnum1[i];

	/* write pattern table */
	fwrite(pnum, 128, 1, out);

	write32b(out, PW_MOD_MAGIC);

	/* a little pre-calc code ... no other way to deal with these unknown
	 * pattern data sizes ! :(
	 */
	hio_seek(in, 4456, SEEK_SET);
	psize = hio_read32b(in);

	/* go back to pattern data starting address */
	hio_seek(in, 5222, SEEK_SET);

	/* now, reading all pattern data to get the max value of note */
	refmax = 0;
	for (j = 0; j < psize; j += 2) {
		int x = hio_read16b(in);
		if (x > refmax)
			refmax = x;
		if (hio_error(in))
			return -1;
	}

	/* read "reference Table" */
	refmax++;		/* coz 1st value is 0 ! */
	refsize = refmax * 4;	/* coz each block is 4 bytes long */
	if ((reftab = (uint8 *)malloc(refsize)) == NULL) {
		return -1;
	}

	if (hio_read(reftab, refsize, 1, in) < 1) {
		goto err;
	}

	/* go back to pattern data starting address */
	hio_seek(in, 5222, SEEK_SET);

	for (j = 0; j <= pat_max; j++) {
		int flag = 0;
		for (i = 0; i < 64; i++) {
			for (k = 0; k < 4; k++) {
				uint8 *p = &pat[j][i * 16 + k * 4];
				int x = hio_read16b(in) << 2;
				int fine, ins, per, fxt;

				/* Sanity check */
				if (x >= refsize || hio_error(in)) {
					goto err;
				}

				memcpy(p, &reftab[x], 4);

				ins = ((p[2] >> 4) & 0x0f) | (p[0] & 0xf0);
				if (ins != 0) {
					oldins[k] = ins;
				}

				per = ((p[0] & 0x0f) << 8) | p[1];
				fxt = p[2] & 0x0f;
				if (oldins[k] > 0 && oldins[k] < 32) {
					fine = fin[oldins[k] - 1];
				} else {
					fine = 0;
				}

				/* Sanity check */
				if (fine >= 16) {
					goto err;
				}

				if (per != 0 && oldins[k] > 0 && fine != 0) {
					for (l = 0; l < 36; l++) {
						if (tun_table[fine][l] == per) {
							p[0] &= 0xf0;
							p[0] |=
						     	    ptk_table[l + 1][0];
							p[1] =
							    ptk_table[l + 1][1];
							break;
						}
					}
				}

				if (fxt == 0x0d || fxt == 0x0b) {
					flag = 1;
				}
			}

			if (flag == 1) {
				break;
			}
		}
		fwrite(pat[j], 1024, 1, out);
	}

	free(reftab);

	hio_seek(in, 4452, SEEK_SET);
	smp_ofs = hio_read32b(in);
	hio_seek(in, 4456 + smp_ofs, SEEK_SET);

	pw_move_data(out, in, ssize);

	return 0;

    err:
	free(reftab);
	return -1;
}

static int test_p10c(const uint8 *data, char *t, int s)
{
	uint8 magic[] = {
		0x60, 0x38, 0x60, 0x00, 0x00, 0xa0, 0x60, 0x00,
		0x01, 0x3e, 0x60, 0x00, 0x01, 0x0c, 0x48, 0xe7
	};

	/* test 1 */
	PW_REQUEST_DATA(s, 22);

	if (memcmp(data, magic, 16) != 0)
		return -1;

	/* test 2 */
	if (data[21] != 0xce)
		return -1;

	PW_REQUEST_DATA(s, 4714);

#if 0
	/* test 3 */
	j = readmem32b(data + 4452);
	if (j + 4452 > in_size)
		return -1;
#endif

	/* test 4 */
	if (readmem16b(data + 4712) & 0x03)
		return -1;

	/* test 5 */
	if (data[36] != 0x10)
		return -1;

	/* test 6 */
	if (data[37] != 0xfc)
		return -1;

	pw_read_title(NULL, t, 0);

	return 0;
}

const struct pw_format pw_p10c = {
	"Promizer 1.0c",
	test_p10c,
	depack_p10c
};

