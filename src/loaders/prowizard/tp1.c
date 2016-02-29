/*
 * TrackerPacker_v1.c  Copyright (C) 1998 Asle / ReDoX
 *
 * Converts TP1 packed MODs back to PTK MODs
 *
 * Modified in 2016 by Claudio Matsuoka
 */

#include <string.h>
#include <stdlib.h>
#include "prowiz.h"

static int depack_tp1(HIO_HANDLE *in, FILE *out)
{
	uint8 c1, c2, c3, c4;
	uint8 pnum[128];
	uint8 pdata[1024];
	uint8 note, ins, fxt, fxp;
	uint8 npat = 0x00;
	uint8 len;
	int i, j;
	int pat_ofs = 999999;
	int paddr[128];
	int paddr_ord[128];
	int size, ssize = 0;
	int smp_ofs;

	memset(paddr, 0, 128 * 4);
	memset(paddr_ord, 0, 128 * 4);
	memset(pnum, 0, 128);

	hio_read32b(in);			/* skip magic */
	hio_read32b(in);			/* skip size */
	pw_move_data(out, in, 20);		/* title */
	smp_ofs = hio_read32b(in);		/* sample data address */

	for (i = 0; i < 31; i++) {
		pw_write_zero(out, 22);		/* sample name */

		c3 = hio_read8(in);		/* read finetune */
		c4 = hio_read8(in);		/* read volume */

		write16b(out, size = hio_read16b(in)); /* size */
		ssize += size * 2;

		write8(out, c3);		/* write finetune */
		write8(out, c4);		/* write volume */

		write16b(out, hio_read16b(in));	/* loop start */
		write16b(out, hio_read16b(in));	/* loop size */
	}

	/* read size of pattern table */
	len = hio_read16b(in) + 1;
	write8(out, len);

	/* ntk byte */
	write8(out, 0x7f);

	for (i = 0; i < len; i++) {
		paddr[i] = hio_read32b(in);
		if (hio_error(in)) {
			return -1;
		}
		if (pat_ofs > paddr[i]) {
			pat_ofs = paddr[i];
		}
	}

	/* ordering of pattern addresses */
	pnum[0] = 0;
	paddr_ord[0] = paddr[0];
	npat = 1;

	for (i = 1; i < len; i++) {
		for (j = 0; j < i; j++) {
			if (paddr[i] == paddr[j]) {
				pnum[i] = pnum[j];
				break;
			}
		}
		if (j == i) {
			paddr_ord[npat] = paddr[i];
			pnum[i] = npat++;
		}
	}

	fwrite(pnum, 128, 1, out);		/* write pattern list */
	write32b(out, PW_MOD_MAGIC);		/* ID string */

	/* pattern datas */
	for (i = 0; i < npat; i++) {
		if (hio_seek(in, 794 + paddr_ord[i] - pat_ofs, SEEK_SET) < 0) {
			return -1;
		}
		memset(pdata, 0, 1024);
		for (j = 0; j < 256; j++) {
			uint8 *p = pdata + j * 4;

			c1 = hio_read8(in);
			if (c1 == 0xc0) {
				continue;
			}
			if ((c1 & 0xc0) == 0x80) {
				fxt = (c1 >> 2) & 0x0f;
				fxp = hio_read8(in);
				p[2] = fxt;
				p[3] = fxp;
				continue;
			}

			c2 = hio_read8(in);
			c3 = hio_read8(in);

			note = (c1 & 0xfe) >> 1;

			if (note > 36) {
				return -1;
			}
			
			ins = ((c2 >> 4) & 0x0f) | ((c1 << 4) & 0x10);
			fxt = c2 & 0x0f;
			fxp = c3;

			p[0] = (ins & 0xf0) | ptk_table[note][0];
			p[1] = ptk_table[note][1];
			p[2] = ((ins << 4) & 0xf0) | fxt;
			p[3] = fxp;
		}

		fwrite(pdata, 1024, 1, out);
	}

	/* Sample data */
	if (hio_seek(in, smp_ofs, SEEK_SET) < 0) {
		return -1;
	}
	pw_move_data(out, in, ssize);

	return 0;
}

static int test_tp1(uint8 *data, char *t, int s)
{
	int i;
	int len, size, smp_ofs;

	PW_REQUEST_DATA(s, 1024);

	if (memcmp(data, "MEXX", 4)) {
		return -1;
	}

	/* size of the module */
	size = readmem32b(data + 4);
	if (size < 794 || size > 2129178) {
		return -1;
	}

	for (i = 0; i < 31; i++) {
		uint8 *d = data + i * 8 + 32;

		/* test finetunes */
		if (d[0] > 0x0f)
			return -1;

		/* test volumes */
		if (d[1] > 0x40)
			return -1;
	}

	/* sample data address */
	smp_ofs = readmem32b(data + 28);
	if (smp_ofs == 0 || smp_ofs > size) {
		return -1;
	}

	/* test sample sizes */
	for (i = 0; i < 31; i++) {
		uint8 *d = data + i * 8 + 32;
		int len = readmem16b(d + 2) << 1;	/* size */
		int start = readmem16b(d + 4) << 1;	/* loop start */
		int lsize = readmem16b(d + 6) << 1;	/* loop size */

		if (len > 0xffff || start > 0xffff || lsize > 0xffff)
			return -1;

		if (start + lsize > len + 2)
			return -1;

		if (start != 0 && lsize == 0)
			return -1;
	}

	/* pattern list size */
	len = data[281];
	if (len == 0 || len > 128) {
		return -1;
	}

	return 0;
}

const struct pw_format pw_tp1 = {
	"Tracker Packer v1",
	test_tp1,
	depack_tp1
};
