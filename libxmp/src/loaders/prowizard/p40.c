/* ProWizard
 * Copyright (C) 1997 Asle / ReDoX
 * Copyright (C) 2007 Claudio Matsuoka
 * Modified in 2021 by Alice Rowan.
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
 * The_Player_4.0.c
 *
 * The Player 4.0a and 4.0b to Protracker.
 */

#include "prowiz.h"

#define MAGIC_P40A	MAGIC4('P','4','0','A')
#define MAGIC_P40B	MAGIC4('P','4','0','B')
#define MAGIC_P41A	MAGIC4('P','4','1','A')


static int set_event(uint8 *x, uint8 c1, uint8 c2, uint8 c3)
{
	uint8 mynote;
	uint8 b;

	mynote = c1 & 0x7f;

	if (PTK_IS_VALID_NOTE(mynote / 2)) {
		*x++ = ((c1 << 4) & 0x10) | ptk_table[mynote / 2][0];
		*x++ = ptk_table[mynote / 2][1];
	} else {
		return -1;
	}

	b = c2 & 0x0f;
	if (b == 0x08)
		c2 -= 0x08;

	*x++ = c2;

	if (b == 0x05 || b == 0x06 || b == 0x0a)
		c3 = c3 > 0x7f ? (c3 << 4) & 0xf0 : c3;

	*x++ = c3;

	return 0;
}

#define track(p,c,r) tdata[((int)(p) * 4 + (c)) * 256 + (r) * 4]


struct smp {
	uint8 name[22];
	int addr;
	uint16 size;
	int loop_addr;
	uint16 loop_size;
	int16 fine;
	uint8 vol;
};

static int depack_p4x(HIO_HANDLE *in, FILE *out)
{
	uint8 c1, c2, c3, c4, c5;
	uint8 tmp[1024];
	uint8 len, nsmp;
	uint8 *tdata;
	uint16 track_addr[128][4];
	long in_size;
	int trkdat_ofs, trktab_ofs, smp_ofs;
	/* int ssize = 0; */
	int SampleAddress[31];
	int SampleSize[31];
	int i, j, k, l, a, b, c;
	struct smp ins;
	uint32 id;

	memset(track_addr, 0, sizeof(track_addr));
	memset(SampleAddress, 0, sizeof(SampleAddress));
	memset(SampleSize, 0, sizeof(SampleSize));

	id = hio_read32b(in);
#if 0
	if (id == MAGIC_P40A) {
		pw_p4x.id = "P40A";
		pw_p4x.name = "The Player 4.0A";
	} else if (id == MAGIC_P40B) {
		pw_p4x.id = "P40B";
		pw_p4x.name = "The Player 4.0B";
	} else {
		pw_p4x.id = "P41A";
		pw_p4x.name = "The Player 4.1A";
	}
#endif

	hio_read8(in);			/* read Real number of pattern */
	len = hio_read8(in);		/* read number of patterns in list */

	/* Sanity check */
	if (len >= 128) {
		return -1;
	}

	nsmp = hio_read8(in);		/* read number of samples */

	/* Sanity check */
	if (nsmp > 31) {
		return -1;
	}

	hio_read8(in);			/* bypass empty byte */
	trkdat_ofs = hio_read32b(in);	/* read track data address */
	trktab_ofs = hio_read32b(in);	/* read track table address */
	smp_ofs = hio_read32b(in);	/* read sample data address */

	if (hio_error(in) || trkdat_ofs < 0 || trktab_ofs < 0 || smp_ofs < 0) {
		return -1;
	}

	in_size = hio_size(in);
	if (trkdat_ofs >= in_size || trktab_ofs >= in_size || smp_ofs >= in_size) {
		return -1;
	}

	pw_write_zero(out, 20);		/* write title */

	/* sample headers stuff */
	for (i = 0; i < nsmp; i++) {
		ins.addr = hio_read32b(in);		/* sample address */
		SampleAddress[i] = ins.addr;
		ins.size = hio_read16b(in);		/* sample size */
		SampleSize[i] = ins.size * 2;
		/* ssize += SampleSize[i]; */
		ins.loop_addr = hio_read32b(in);	/* loop start */
		ins.loop_size = hio_read16b(in);	/* loop size */
		ins.fine = 0;
		if (id == MAGIC_P40A || id == MAGIC_P40B)
			ins.fine = hio_read16b(in);	/* finetune */
		hio_read8(in);				/* bypass 00h */
		ins.vol = hio_read8(in);		/* read vol */
		if (id == MAGIC_P41A)
			ins.fine = hio_read16b(in);	/* finetune */

		/* Sanity check */
		if (ins.addr < 0 || ins.loop_addr < 0 || ins.loop_addr < ins.addr ||
		    ins.addr > in_size - smp_ofs) {
			return -1;
		}

		/* writing now */
		pw_write_zero(out, 22);			/* sample name */
		write16b(out, ins.size);
		write8(out, ins.fine / 74);
		write8(out, ins.vol);
		write16b(out, (ins.loop_addr - ins.addr) / 2);
		write16b(out, ins.loop_size);
	}

	/* go up to 31 samples */
	memset(tmp, 0, 30);
	tmp[29] = 0x01;
	for (; i < 31; i++)
		fwrite (tmp, 30, 1, out);

	write8(out, len);		/* write size of pattern list */
	write8(out, 0x7f);		/* write noisetracker byte */

	hio_seek(in, trktab_ofs + 4, SEEK_SET);

	for (c1 = 0; c1 < len; c1++)	/* write pattern list */
		write8(out, c1);
	for (; c1 < 128; c1++)
		write8(out, 0);

	write32b(out, PW_MOD_MAGIC);	/* write ptk ID */

	for (i = 0; i < len; i++) {	/* read all track addresses */
		for (j = 0; j < 4; j++)
			track_addr[i][j] = hio_read16b(in) + trkdat_ofs + 4;
	}

	hio_seek(in, trkdat_ofs + 4, SEEK_SET);

	if ((tdata = (uint8 *)calloc(512, 256)) == NULL) {
		return -1;
	}

	for (i = 0; i < len; i++) {	/* rewrite the track data */
		for (j = 0; j < 4; j++) {
			hio_seek(in, track_addr[i][j], SEEK_SET);

			for (k = 0; k < 64; k++) {
				c1 = hio_read8(in);
				c2 = hio_read8(in);
				c3 = hio_read8(in);
				c4 = hio_read8(in);

				if (c1 != 0x80) {
					uint8 *tr = &track(i, j, k);
					if (hio_error(in) || set_event(tr, c1, c2, c3) < 0)
						goto err;

					if ((c4 > 0x00) && (c4 < 0x80))
						k += c4;
					if (c4 > 0x7f) {
						k++;
						for (l = 256; l > c4; l--) {
							tr = &track(i, j, k);
							if (k >= 64)
								goto err;

							set_event(tr, c1, c2, c3);
							k++;
						}
						k--;
					}
					continue;
				}

				if ((a = hio_tell(in)) < 0) {
					goto err;
				}

				c5 = c2;
				b = (c3 << 8) + c4 + trkdat_ofs + 4;

				hio_seek(in, b, SEEK_SET);

				for (c = 0; c <= c5; c++) {
					uint8 *tr = &track(i, j, k);
					c1 = hio_read8(in);
					c2 = hio_read8(in);
					c3 = hio_read8(in);
					c4 = hio_read8(in);

					if (hio_error(in) || k >= 64 || set_event(tr, c1, c2, c3) < 0)
						goto err;

					if ((c4 > 0x00) && (c4 < 0x80))
						k += c4;
					if (c4 > 0x7f) {
						k++;
						for (l = 256; l > c4; l--) {
							tr = &track(i, j, k);
							if (k >= 64)
								goto err;

							set_event(tr, c1, c2, c3);
							k++;
						}
						k--;
					}
					k++;
				}
				k--;
				hio_seek(in, a, SEEK_SET);
			}
		}
	}

	/* write pattern data */
	for (i = 0; i < len; i++) {
		memset(tmp, 0, sizeof(tmp));
		for (j = 0; j < 64; j++) {
			for (k = 0; k < 4; k++) {
				uint8 *tr = &track(i, k, j);
				int x = j * 16 + k * 4;

				tmp[x + 0] = tr[0];
				tmp[x + 1] = tr[1];
				tmp[x + 2] = tr[2];
				tmp[x + 3] = tr[3];
			}
		}
		fwrite(tmp, 1024, 1, out);
	}

	/* read and write sample data */
	for (i = 0; i < nsmp; i++) {
		hio_seek(in, SampleAddress[i] + smp_ofs, SEEK_SET);
		pw_move_data(out, in, SampleSize[i]);
	}

	free(tdata);
	return 0;
    err:
	free(tdata);
	return -1;
}

static int test_p4x(const uint8 *data, char *t, int s)
{
	//int j, k, l, o, n;
	//int start = 0, ssize;
	uint32 id;

	PW_REQUEST_DATA(s, 8);

	id = readmem32b(data);

	if (id != MAGIC_P40A && id != MAGIC_P40B && id != MAGIC_P41A)
		return -1;

	pw_read_title(NULL, t, 0);

	return 0;

#if 0
	/* number of pattern (real) */
	j = data[start + 4];
	if (j > 0x7f)
		return -1;

	/* number of sample */
	k = data[start + 6];
	if ((k > 0x1F) || (k == 0))
		return -1;

	/* test volumes */
	for (l = 0; l < k; l++) {
		if (data[start + 35 + l * 16] > 0x40)
			return -1;
	}

	/* test sample sizes */
	ssize = 0;
	for (l = 0; l < k; l++) {
		/* size */
		o = (data[start + 24 + l * 16] << 8) +
			data[start + 25 + l * 16];
		/* loop size */
		n = (data[start + 30 + l * 16] << 8) +
			data[start + 31 + l * 16];
		o *= 2;
		n *= 2;

		if ((o > 0xFFFF) || (n > 0xFFFF))
			return -1;

		if (n > (o + 2))
			return -1;

		ssize += o;
	}
	if (ssize <= 4)
		return -1;

	/* ssize is the size of the sample data .. WRONG !! */
	/* k is the number of samples */
	return 0;
#endif
}

const struct pw_format pw_p4x = {
	"The Player 4.x",
	test_p4x,
	depack_p4x
};

