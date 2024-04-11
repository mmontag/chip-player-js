/*
 * Based on the public domain version by Olivier Lapicque
 * Rewritten for libxmp by Claudio Matsuoka
 *
 * Copyright (C) 2012 Claudio Matsuoka
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

#include "../common.h"
#include "depacker.h"

#define MMCMP_COMP	0x0001
#define MMCMP_DELTA	0x0002
#define MMCMP_16BIT	0x0004
#define MMCMP_STEREO	0x0100
#define MMCMP_ABS16	0x0200
#define MMCMP_ENDIAN	0x0400

struct header {
	int version;
	int nblocks;
	int filesize;
	int blktable;
	int glb_comp;
	int fmt_comp;
};

struct block {
	int unpk_size;
	int pk_size;
	int xor_chk;
	int sub_blk;
	int flags;
	int tt_entries;
	int num_bits;
};

struct sub_block {
	int unpk_pos;
	int unpk_size;
};

static const uint32 cmd_8bits[8] = {
	0x01, 0x03, 0x07, 0x0f, 0x1e, 0x3c, 0x78, 0xf8
};

static const uint32 fetch_8bit[8] = {
	3, 3, 3, 3, 2, 1, 0, 0
};

static const uint32 cmd_16bit[16] = {
	0x0001, 0x0003, 0x0007, 0x000f, 0x001e, 0x003c, 0x0078, 0x00f0,
	0x01f0, 0x03f0, 0x07f0, 0x0ff0, 0x1ff0, 0x3ff0, 0x7ff0, 0xfff0
};

static const uint32 fetch_16bit[16] = {
	4, 4, 4, 4, 3, 2, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

struct bit_buffer {
	uint32 count;
	uint32 buffer;
};

static uint32 get_bits(HIO_HANDLE *f, int n, struct bit_buffer *bb)
{
	uint32 bits;

	if (n == 0) {
		return 0;
	}

	while (bb->count < 24) {
		bb->buffer |= hio_read8(f) << bb->count;
		bb->count += 8;
	}

	bits = bb->buffer & ((1 << n) - 1);
	bb->buffer >>= n;
	bb->count -= n;

	return bits;
}

struct mem_buffer {
	uint8 *buf;
	size_t size;
	size_t pos;
};

static int mem_seek(struct mem_buffer *out, int pos_set)
{
	if (pos_set >= out->size)
		return -1;

	out->pos = pos_set;
	return 0;
}

static int mem_write8(uint8 value, struct mem_buffer *out)
{
	if (out->pos >= out->size)
		return -1;

	out->buf[out->pos++] = value;
	return 0;
}

static int mem_write16l(uint16 value, struct mem_buffer *out)
{
	/* Some MMCMP blocks seem to rely on writing half words. This
	 * theoretically could occur at the end of the file, so write each
	 * byte separately. */
	if (mem_write8(value & 0xff, out) ||
	    mem_write8(value >> 8, out))
		return -1;

	return 0;
}

static int block_copy(struct block *block, struct sub_block *sub,
		      HIO_HANDLE *in, struct mem_buffer *out)
{
	int i;

	for (i = 0; i < block->sub_blk; i++, sub++) {
		if (sub->unpk_pos >= out->size ||
		    sub->unpk_size > out->size - sub->unpk_pos)
			return -1;

		if (hio_read(out->buf + sub->unpk_pos, 1, sub->unpk_size, in) < sub->unpk_size)
			return -1;
	}
	return 0;
}

static int block_unpack_16bit(struct block *block, struct sub_block *sub,
			      HIO_HANDLE *in, struct mem_buffer *out)
{
	struct bit_buffer bb;
	uint32 pos = 0;
	uint32 numbits = block->num_bits;
	uint32 j, oldval = 0;

	bb.count = 0;
	bb.buffer = 0;

	if (mem_seek(out, sub->unpk_pos) < 0) {
		return -1;
	}
	if (hio_seek(in, block->tt_entries, SEEK_CUR) < 0) {
		return -1;
	}

	for (j = 0; j < block->sub_blk; ) {
		uint32 size = sub[j].unpk_size;
		uint32 newval = 0x10000;
		uint32 d = get_bits(in, numbits + 1, &bb);

		if (d >= cmd_16bit[numbits]) {
			uint32 fetch = fetch_16bit[numbits];
			uint32 newbits = get_bits(in, fetch, &bb) +
					((d - cmd_16bit[numbits]) << fetch);

			if (newbits != numbits) {
				numbits = newbits & 0x0f;
			} else {
				if ((d = get_bits(in, 4, &bb)) == 0x0f) {
					if (get_bits(in, 1, &bb))
						break;
					newval = 0xffff;
				} else {
					newval = 0xfff0 + d;
				}
			}
		} else {
			newval = d;
		}

		if (newval < 0x10000) {
			if (newval & 1) {
				newval = (uint32)(-(int32)((newval + 1) >> 1));
			} else {
				newval = (uint32)(newval >> 1);
			}

			if (block->flags & MMCMP_DELTA) {
				newval += oldval;
				oldval = newval;
			} else if (!(block->flags & MMCMP_ABS16)) {
				newval ^= 0x8000;
			}

			pos += 2;
			mem_write16l((uint16)newval, out);
		}

		if (pos >= size) {
			if (++j >= block->sub_blk)
				break;

			pos = 0;
			if (mem_seek(out, sub[j].unpk_pos) < 0) {
				return -1;
			}
		}
	}

	return 0;
}

static int block_unpack_8bit(struct block *block, struct sub_block *sub,
			     HIO_HANDLE *in, struct mem_buffer *out)
{
	struct bit_buffer bb;
	uint32 pos = 0;
	uint32 numbits = block->num_bits;
	uint32 j, oldval = 0;
	uint8 ptable[0x100];
	long seekpos = hio_tell(in) + block->tt_entries;

	/* The way the original libmodplug depacker is written allows values
	 * to be read from the compressed data. It's impossible to tell if this
	 * was intentional or yet another bug. Nothing seems to rely on it. */
	memset(ptable, 0, sizeof(ptable));
	if (hio_read(ptable, 1, 0x100, in) < block->tt_entries) {
		return -1;
	}

	bb.count = 0;
	bb.buffer = 0;

	if (mem_seek(out, sub->unpk_pos) < 0) {
		return -1;
	}
	if (hio_seek(in, seekpos, SEEK_SET) < 0) {
		return -1;
	}

	for (j = 0; j < block->sub_blk; ) {
		uint32 size = sub[j].unpk_size;
		uint32 newval = 0x100;
		uint32 d = get_bits(in, numbits+1, &bb);

		if (d >= cmd_8bits[numbits]) {
			uint32 fetch = fetch_8bit[numbits];
			uint32 newbits = get_bits(in, fetch, &bb) +
					((d - cmd_8bits[numbits]) << fetch);

			if (newbits != numbits) {
				numbits = newbits & 0x07;
			} else {
				if ((d = get_bits(in, 3, &bb)) == 7) {
					if (get_bits(in, 1, &bb))
						break;
					newval = 0xff;
				} else {
					newval = 0xf8 + d;
				}
			}
		} else {
			newval = d;
		}

		if (newval < 0x100) {
			int n = ptable[newval];
			if (block->flags & MMCMP_DELTA) {
				n += oldval;
				oldval = n;
			}

			pos++;
			mem_write8((uint8)n, out);
		}

		if (pos >= size) {
			if (++j >= block->sub_blk)
				break;

			pos = 0;
			if (mem_seek(out, sub[j].unpk_pos) < 0) {
				return -1;
			}
		}
	}

	return 0;
}

static int test_mmcmp(unsigned char *b)
{
	return memcmp(b, "ziRCONia", 8) == 0;
}

static int decrunch_mmcmp(HIO_HANDLE *in, void **out, long inlen, long *outlen)
{
	struct header h;
	struct mem_buffer outbuf;
	uint32 *table;
	uint32 i, j;

	/* Read file header */
	if (hio_read32l(in) != 0x4352697A)		/* ziRC */
		goto err;
	if (hio_read32l(in) != 0x61694e4f)		/* ONia */
		goto err;
	if (hio_read16l(in) != 14)			/* header size */
		goto err;

	/* Read header */
	h.version = hio_read16l(in);
	if (hio_error(in) != 0) goto err;
	h.nblocks = hio_read16l(in);
	if (hio_error(in) != 0) goto err;
	h.filesize = hio_read32l(in);
	if (hio_error(in) != 0) goto err;
	h.blktable = hio_read32l(in);
	if (hio_error(in) != 0) goto err;
	h.glb_comp = hio_read8(in);
	if (hio_error(in) != 0) goto err;
	h.fmt_comp = hio_read8(in);
	if (hio_error(in) != 0) goto err;

	if (h.nblocks == 0 || h.filesize < 16)
		goto err;

	/* Block table */
	if (hio_seek(in, h.blktable, SEEK_SET) < 0) {
		goto err;
	}

	table = (uint32 *) malloc(h.nblocks * 4);
	if (table == NULL) {
		goto err;
	}
	outbuf.buf = (uint8 *) calloc(1, h.filesize);
	if (outbuf.buf == NULL) {
		goto err2;
	}
	outbuf.pos = 0;
	outbuf.size = h.filesize;

	for (i = 0; i < h.nblocks; i++) {
		table[i] = hio_read32l(in);
		if (hio_error(in) != 0) goto err2;
	}

	for (i = 0; i < h.nblocks; i++) {
		struct block block;
		struct sub_block *sub_block;
		uint8 buf[20];

		if (hio_seek(in, table[i], SEEK_SET) < 0) {
			goto err2;
		}

		if (hio_read(buf, 1, 20, in) != 20) {
			goto err2;
		}

		block.unpk_size  = readmem32l(buf);
		block.pk_size    = readmem32l(buf + 4);
		block.xor_chk    = readmem32l(buf + 8);
		block.sub_blk    = readmem16l(buf + 12);
		block.flags      = readmem16l(buf + 14);
		block.tt_entries = readmem16l(buf + 16);
		block.num_bits   = readmem16l(buf + 18);

		/* Sanity check */
		if (block.unpk_size <= 0 || block.pk_size <= 0)
			goto err2;
		if (block.tt_entries < 0 || block.pk_size <= block.tt_entries)
			goto err2;
		if (block.sub_blk <= 0)
			goto err2;
		if (block.flags & MMCMP_COMP) {
			if (block.flags & MMCMP_16BIT) {
				if (block.num_bits >= 16) {
					goto err2;
				}
			} else {
				if (block.num_bits >= 8) {
					goto err2;
				}
			}
		}

		sub_block = (struct sub_block *) malloc(block.sub_blk * sizeof (struct sub_block));
		if (sub_block == NULL)
			goto err2;

		for (j = 0; j < block.sub_blk; j++) {
			if (hio_read(buf, 1, 8, in) != 8) {
				free(sub_block);
				goto err2;
			}

			sub_block[j].unpk_pos  = readmem32l(buf);
			sub_block[j].unpk_size = readmem32l(buf + 4);

			/* Sanity check */
			if (sub_block[j].unpk_pos < 0 ||
			    sub_block[j].unpk_size < 0) {
				free(sub_block);
				goto err2;
			}
		}

		if (~block.flags & MMCMP_COMP) {
			/* Data is not packed */
			if (block_copy(&block, sub_block, in, &outbuf) < 0) {
				free(sub_block);
				goto err2;
			}
		} else if (block.flags & MMCMP_16BIT) {
			/* Data is 16-bit packed */
			if (block_unpack_16bit(&block, sub_block, in, &outbuf) < 0) {
				free(sub_block);
				goto err2;
			}
		} else {
			/* Data is 8-bit packed */
			if (block_unpack_8bit(&block, sub_block, in, &outbuf) < 0) {
				free(sub_block);
				goto err2;
			}
		}

		free(sub_block);
	}

	*out = outbuf.buf;
	*outlen = h.filesize;

	free(table);
	return 0;

    err2:
	free(outbuf.buf);
	free(table);
    err:
	return -1;
}

struct depacker libxmp_depacker_mmcmp = {
	test_mmcmp,
	NULL,
	decrunch_mmcmp
};
