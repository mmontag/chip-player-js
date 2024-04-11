/* ProWizard
 * Copyright (C) 1997 Sylvain "Asle" Chipaux
 * Modified in 2006,2009,2014 by Claudio Matsuoka
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
 * StarTrekker_Packer.c
 *
 * Converts back to ptk StarTrekker packed MODs
 */

#include "prowiz.h"


static int depack_starpack(HIO_HANDLE *in, FILE *out)
{
	uint8 pnum[128];
	uint8 pnum_tmp[128];
	uint8 pat_pos;
	uint8 buffer[1024];
	uint8 num_pat = 0x00;
	int i = 0, j = 0, k = 0;
	int size, ssize = 0;
	int paddr[128];
	int paddr_tmp[128];
	int paddr_tmp2[128];
	int tmp_ptr, tmp1, tmp2;
	int smp_addr = 0;
	int spaces_left;

	memset(pnum, 0, sizeof(pnum));
	memset(pnum_tmp, 0, sizeof(pnum_tmp));
	memset(paddr, 0, sizeof(paddr));
	memset(paddr_tmp, 0, sizeof(paddr_tmp));
	memset(paddr_tmp2, 0, sizeof(paddr_tmp2));

	pw_move_data(out, in, 20);		/* title */

	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);		/* sample name */
		write16b(out, size = hio_read16b(in));	/* size */
		ssize += 2 * size;
		write8(out, hio_read8(in));	/* finetune */
		write8(out, hio_read8(in));	/* volume */
		write16b(out, hio_read16b(in));	/* loop start */
		write16b(out, hio_read16b(in));	/* loop size */
	}

	pat_pos = hio_read16b(in) >> 2;		/* num positions = size of pattern table / 4 */

	if (pat_pos >= 128) {
		return -1;
	}

	hio_seek(in, 2, SEEK_CUR);		/* bypass $0000 unknown bytes */

	for (i = 0; i < 128; i++) {
		paddr[i] = hio_read32b(in);
	}

	/* ordering of patterns addresses */

	tmp_ptr = 0;
	for (i = 0; i < pat_pos; i++) {
		if (i == 0) {
			pnum[0] = 0;
			tmp_ptr++;
			continue;
		}

		for (j = 0; j < i; j++) {
			if (paddr[i] == paddr[j]) {
				pnum[i] = pnum[j];
				break;
			}
		}
		if (j == i)
			pnum[i] = tmp_ptr++;
	}

	for (i = 0; i < 128; i++)
		paddr_tmp[i] = paddr[i];

      restart:
	for (i = 0; i < pat_pos; i++) {
		for (j = 0; j < i; j++) {
			if (paddr_tmp[i] < paddr_tmp[j]) {
				tmp2 = pnum[j];
				pnum[j] = pnum[i];
				pnum[i] = tmp2;
				tmp1 = paddr_tmp[j];
				paddr_tmp[j] = paddr_tmp[i];
				paddr_tmp[i] = tmp1;
				goto restart;
			}
		}
	}

	j = 0;
	for (i = 0; i < 128; i++) {
		if (i == 0) {
			paddr_tmp2[j] = paddr_tmp[i];
			continue;
		}

		if (paddr_tmp[i] == paddr_tmp2[j])
			continue;
		paddr_tmp2[++j] = paddr_tmp[i];
	}

	/* try to locate unused patterns .. hard ! */
	spaces_left = 128 - pat_pos;
	j = 0;
	for (i = 0; i < (pat_pos - 1); i++) {
		paddr_tmp[j] = paddr_tmp2[i];
		j += 1;
		if ((paddr_tmp2[i + 1] - paddr_tmp2[i]) > 1024 && spaces_left > 0) {
			/*printf ( "! pattern %ld is not used ... saved anyway\n" , j ); */
			paddr_tmp[j] = paddr_tmp2[i] + 1024;
			spaces_left--;
			j += 1;
		}
	}

	/* assign pattern list */
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++)
			if (paddr[i] == paddr_tmp[j]) {
				pnum_tmp[i] = j;
				break;
			}
	}

	memset(pnum, 0, sizeof(pnum));
	for (i = 0; i < pat_pos; i++) {
		pnum[i] = pnum_tmp[i];
	}

	write8(out, pat_pos);			/* write number of position */

	/* get highest pattern number */
	for (i = 0; i < pat_pos; i++) {
		if (pnum[i] > num_pat)
			num_pat = pnum[i];
	}

	write8(out, 0x7f);			/* write noisetracker byte */
	fwrite(pnum, 128, 1, out);		/* write pattern list */
	write32b(out, PW_MOD_MAGIC);		/* M.K. */

	/* read sample data address */
	hio_seek(in, 0x310, SEEK_SET);
	smp_addr = hio_read32b(in) + 0x314;

	/* pattern data */
	num_pat += 1;
	for (i = 0; i < num_pat; i++) {
		memset(buffer, 0, sizeof(buffer));
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				uint8 c1, c2, c3, c4, c5;
				int ofs = j * 16 + k * 4;

				c1 = hio_read8(in);
				if (c1 == 0x80)
					continue;
				c2 = hio_read8(in);
				c3 = hio_read8(in);
				c4 = hio_read8(in);
				buffer[ofs] = c1 & 0x0f;
				buffer[ofs + 1] = c2;
				buffer[ofs + 2] = c3 & 0x0f;
				buffer[ofs + 3] = c4;

				c5 = ((c1 & 0xf0) | ((c3 >> 4) & 0x0f)) >> 2;
				buffer[ofs] |= c5 & 0xf0;
				buffer[ofs + 2] |= (c5 << 4) & 0xf0;
			}
		}
		fwrite(buffer, 1024, 1, out);
		/*printf ( "+" ); */
	}
	/*printf ( "\n" ); */

	/* sample data */
	hio_seek(in, smp_addr, 0);
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_starpack(const uint8 *data, char *t, int s)
{
	int i;
	int plist_size, len, sdata_ofs, pdata_ofs;

	PW_REQUEST_DATA(s, 788);

	/* test 2 */
	plist_size = readmem16b(data + 268);
	if (plist_size & 0x03)
		return -1;

	len = plist_size >> 2;
	if (len == 0 || len > 127)
		return -1;

	if (data[784] != 0)
		return -1;

	/* test #3  smp size < loop start + loop size ? */
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		int size = readmem16b(d + 20) << 1;
		int lend = (readmem16b(d + 24) + readmem16b(d + 26)) << 1;

		if (lend > size + 2)
			return -1;
	}

	/* test #4  finetunes & volumes */
	for (i = 0; i < 31; i++) {
		const uint8 *d = data + i * 8;
		if (d[22] > 0x0f || d[23] > 0x40)
			return -1;
	}

	/* test #5  pattern addresses > sample address ? */
	/* get sample data address */
#if 0
	if ((start + 0x314) > in_size) {
		Test = BAD;
		return;
	}
#endif
	/* address of sample data */
	sdata_ofs = readmem32b(data + 784);

#if 0
	if ((k + start) > in_size) {
		Test = BAD;
		return;
	}
#endif
	if (sdata_ofs < 788)
		return -1;

	/* pattern addresses > sample address ? */
	for (i = 0; i < len; i++) {
		/* each pattern address */
		if (readmem32b(data + i * 4 + 272) > sdata_ofs)
			return -1;
	}

	/* test last patterns of the pattern list == 0 ? */
	for (; i < 128; i++) {
		if (readmem32b(data + i * 4 + 272) != 0)
			return -1;
	}

	PW_REQUEST_DATA(s, sdata_ofs + 4);

	/* test pattern data */
	pdata_ofs = 788;
	while (pdata_ofs < sdata_ofs + 4) {
		const uint8 *d = data + pdata_ofs;

		if (d[0] == 0x80) {
			pdata_ofs++;
			continue;
		}

		if (d[0] > 0x80)
			return -1;

		/* empty row ? ... not possible ! */
		if (readmem32b(d) == 0)
			return - 1;

		/* fx = C .. arg > 64 ? */
		if ((d[2] * 0x0f) == 0x0c && d[3] > 0x40)
			return - 1;

		/* fx = D .. arg > 64 ? */
		if ((d[2] * 0x0f) == 0x0d && d[3] > 0x40)
			return - 1;

		pdata_ofs += 4;
	}

	pw_read_title(data, t, 20);

	return 0;
}

const struct pw_format pw_starpack = {
	"Startrekker Packer",
	test_starpack,
	depack_starpack
};
