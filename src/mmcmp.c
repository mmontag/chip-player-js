/*
 * Based on the original version by  Olivier Lapicque
 * Rewritten for xmp by Claudio Matsuoka
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "common.h"

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
	0x01, 0x03,	0x07, 0x0F,	0x1E, 0x3C,	0x78, 0xF8
};

static const uint32 fetch_8bit[8] = {
	3, 3, 3, 3, 2, 1, 0, 0
};

static const uint32 cmd_16bit[16] = {
	0x01, 0x03,	0x07, 0x0F,	0x1E, 0x3C,	0x78, 0xF0,
	0x1F0, 0x3F0, 0x7F0, 0xFF0, 0x1FF0, 0x3FF0, 0x7FF0, 0xFFF0
};

static const uint32 fetch_16bit[16] = {
	4, 4, 4, 4, 3, 2, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

struct bit_buffer {
	uint32 count;
	uint32 buffer;
};

static uint32 get_bits(FILE *f, int n, struct bit_buffer *bb)
{
	uint32 bits;

	if (n == 0) {
		return 0;
	}

	while (bb->count < 24) {
		bb->buffer |= read8(f) << bb->count;
		bb->count += 8;
	}

	bits = bb->buffer & ((1 << n) - 1);
	bb->buffer >>= n;
	bb->count -= n;

	return bits;
}

static void block_copy(struct block *block, struct sub_block *sub,
		       FILE *in, FILE *out)
{
	int i;

	for (i = 0; i < block->sub_blk; i++, sub++) {
		move_data(out, in, sub->unpk_size);
	}
}

static void block_unpack_16bit(struct block *block, struct sub_block *sub,
			       FILE *in, FILE *out)
{
	struct bit_buffer bb;
	uint32 pos = 0;
	uint32 numbits = block->num_bits;
	uint32 j, oldval = 0;

	bb.count = 0;
	bb.buffer = 0;
	fseek(in, block->tt_entries, SEEK_SET);

	for (j = 0; j < block->sub_blk; ) {
		uint32 size = sub[j].unpk_size >> 1;
		uint32 newval = 0x10000;
		uint32 d = get_bits(in, numbits+1, &bb);

		if (d >= cmd_16bit[numbits]) {
			uint32 fetch = fetch_16bit[numbits];
			uint32 newbits = get_bits(in, fetch, &bb) +
					((d - cmd_16bit[numbits]) << fetch);

			if (newbits != numbits) {
				numbits = newbits & 0x0F;
			} else {
				if ((d = get_bits(in, 4, &bb)) == 0x0F) {
					if (get_bits(in, 1, &bb)) break;
					newval = 0xFFFF;
				} else {
					newval = 0xFFF0 + d;
				}
			}
		} else {
			newval = d;
		}

		if (newval < 0x10000) {
			if (newval & 1) {
				newval = (uint32)(-(int32)((newval+1) >> 1));
			} else {
				newval = (uint32)(newval >> 1);
			}

			if (block->flags & MMCMP_DELTA) {
				newval += oldval;
				oldval = newval;
			} else if (!(block->flags & MMCMP_ABS16)) {
				newval ^= 0x8000;
			}

			pos++;
			write16l(out, newval);
		}

		if (pos >= size) {
			j++;
			pos = 0;
		}
	}
}

static void block_unpack_8bit(struct block *block, struct sub_block *sub,
			      FILE *in, FILE *out)
{
	struct bit_buffer bb;
	uint32 pos = 0;
	uint32 numbits = block->num_bits;
	uint32 j, oldval = 0;
	uint8 ptable[0x100];

	fread(ptable, 1, 0x100, in);

	bb.count = 0;
	bb.buffer = 0;

	fseek(in, block->tt_entries, SEEK_SET);

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
					if (get_bits(in, 1, &bb)) break;
					newval = 0xFF;
				} else {
					newval = 0xF8 + d;
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
			write8(out, n);
		}

		if (pos >= size) {
			j++;
			pos = 0;
		}
	}
}

int decrunch_mmcmp (FILE *in, FILE *out)                          
{
	struct header h;
	uint32 *table;
	uint32 i, j;

	/* Read file header */
	if (read32l(in) != 0x4352697A)		/* ziRC */
		return -1;
	if (read32l(in) != 0x61694e4f)		/* ONia */
		return -1;
	if (read16l(in) < 14)			/* header size */
		return -1;

	/* Read header */
	h.version = read16l(in);
	h.nblocks = read16l(in);
	h.filesize = read32l(in);
	h.blktable = read32l(in);
	h.glb_comp = read8(in);
	h.fmt_comp = read8(in);

	if (h.nblocks == 0)
		return -1;

	/* Block table */
	fseek(in, h.blktable, SEEK_SET);
	table = malloc(h.nblocks * 4);
	if (table == NULL)
		return -1;

	for (i = 0; i < h.nblocks; i++) {
		table[i] = read32l(in);
	}

	for (i = 0; i < h.nblocks; i++) {
		struct block block;
		struct sub_block *sub_block;

		fseek(in, table[i], SEEK_SET);
		block.unpk_size  = read32l(in);
		block.pk_size    = read32l(in);
		block.xor_chk    = read32l(in);
		block.sub_blk    = read16l(in);
		block.flags      = read16l(in);
		block.tt_entries = read16l(in);
		block.num_bits   = read16l(in);

		sub_block = malloc(block.sub_blk * sizeof (struct sub_block));
		if (sub_block == NULL)
			goto err;

		for (j = 0; j < block.sub_blk; j++) {
			sub_block[j].unpk_pos  = read32l(in);
			sub_block[j].unpk_size = read32l(in);
		}

		block.tt_entries += ftell(in);

		if (~block.flags & MMCMP_COMP) {
			/* Data is not packed */
			block_copy(&block, sub_block, in, out);
		} else if (block.flags & MMCMP_16BIT) {
			/* Data is 16-bit packed */
			block_unpack_16bit(&block, sub_block, in, out);
		} else {
			/* Data is 8-bit packed */
			block_unpack_8bit(&block, sub_block, in, out);
		}

		free(sub_block);
	}

	free(table);
	return 0;

    err:
	free(table);
	return -1;
}
