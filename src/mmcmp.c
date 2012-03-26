/*
 * This program is  free software; you can redistribute it  and modify it
 * under the terms of the GNU  General Public License as published by the
 * Free Software Foundation; either version 2  of the license or (at your
 * option) any later version.
 *
 * Author: Olivier Lapicque <olivierl@jps.net>
 * Modified by Claudio Matsuoka for xmp
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "common.h"


struct header {
	uint16 version;
	uint16 nblocks;
	uint32 filesize;
	uint32 blktable;
	uint8 glb_comp;
	uint8 fmt_comp;
};

struct block {
	uint32 unpk_size;
	uint32 pk_size;
	uint32 xor_chk;
	uint16 sub_blk;
	uint16 flags;
	uint16 tt_entries;
	uint16 num_bits;
};

struct sub_block {
	uint32 unpk_pos;
	uint32 unpk_size;
};

#define MMCMP_COMP	0x0001
#define MMCMP_DELTA	0x0002
#define MMCMP_16BIT	0x0004
#define MMCMP_STEREO	0x0100
#define MMCMP_ABS16	0x0200
#define MMCMP_ENDIAN	0x0400

struct bit_buffer {
	uint32 count;
	uint32 buffer;
	uint8 *src;
	uint8 *end;
};


static uint32 get_bits(struct bit_buffer *bb, uint32 bits)
{
	uint32 d;

	if (!bits)
		return 0;

	while (bb->count < 24) {
		bb->buffer |= ((bb->src < bb->end) ?
			*bb->src++ : 0) << bb->count;
		bb->count += 8;
	}

	d = bb->buffer & ((1 << bits) - 1);
	bb->buffer >>= bits;
	bb->count -= bits;

	return d;
}



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



static int mmcmp_unpack(FILE *in, FILE *out)
{
	uint8 *mem;
	uint8 *buffer;
	struct header h;
	uint32 *table;
	uint32 i, j;
	struct stat st;
	int len;

	if (fstat(fileno(in), &st) < 0)
		return -1;

	len = st.st_size;
	if (len < 256)
		return -1;

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

	if (h.nblocks == 0 || h.filesize < 16 || h.filesize > 0x8000000)
		return -1;

	if (h.blktable >= len || h.blktable + 4 * h.nblocks > len)
		return -1;

	mem = malloc(len);
	if (mem == NULL)
		return -1;

	fseek(in, 0, SEEK_SET);
	if (fread(mem, 1, len, in) != len)
		return -1;


	/* Destination buffer */
	buffer = (uint8 *)calloc(1, (h.filesize + 31) & ~15);
	if (buffer == NULL) {
		return -1;
	}

	/* Block table */
	fseek(in, h.blktable, SEEK_SET);
	table = malloc(h.nblocks * 4);
	for (i = 0; i < h.nblocks; i++) {
		table[i] = read32l(in);
	}

	for (i = 0; i < h.nblocks; i++) {
		uint32 mempos = table[i];
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
		for (j = 0; j < block.sub_blk; j++) {
			sub_block[j].unpk_pos  = read32l(in);
			sub_block[j].unpk_size = read32l(in);
			
		}

		if (mempos + 20 >= len ||
		    mempos + 20 + block.sub_blk*8 >= len) {
			 break;
		}
		mempos += 20 + block.sub_blk*8;

		if (~block.flags & MMCMP_COMP) {
			/* Data is not packed */
			for (j = 0; j < block.sub_blk; j++) {
				struct sub_block *sub = &sub_block[j];

				if (sub->unpk_pos > h.filesize ||
				    sub->unpk_pos + sub->unpk_size > h.filesize) {
					break;
				}

				memcpy(buffer + sub->unpk_pos, mem + mempos,
							sub->unpk_size);
				mempos += sub->unpk_size;
			}
		} else if (block.flags & MMCMP_16BIT) {
			/* Data is 16-bit packed */
			struct bit_buffer bb;
			uint32 pos = 0;
			uint32 numbits = block.num_bits;
			uint32 j, oldval = 0;

			bb.count = 0;
			bb.buffer = 0;
			bb.src = mem+mempos+block.tt_entries;
			bb.end = mem+mempos+block.pk_size;

			for (j = 0; j < block.sub_blk; ) {
				uint32 sz = sub_block[j].unpk_size >> 1;
				uint16 *dest = (uint16 *)(buffer + sub_block[j].unpk_pos);

				uint32 newval = 0x10000;
				uint32 d = get_bits(&bb, numbits+1);

				if (d >= cmd_16bit[numbits]) {
					uint32 nFetch = fetch_16bit[numbits];
					uint32 newbits = get_bits(&bb, nFetch) + ((d - cmd_16bit[numbits]) << nFetch);
					if (newbits != numbits) {
						numbits = newbits & 0x0F;
					} else {
						if ((d = get_bits(&bb, 4)) == 0x0F) {
							if (get_bits(&bb,1)) break;
							newval = 0xFFFF;
						} else {
							newval = 0xFFF0 + d;
						}
					}
				} else {
					newval = d;
				}

				if (newval < 0x10000) {
					newval = (newval & 1) ? (uint32)(-(int32)((newval+1) >> 1)) : (uint32)(newval >> 1);
					if (block.flags & MMCMP_DELTA) {
						newval += oldval;
						oldval = newval;
					} else if (!(block.flags & MMCMP_ABS16)) {
						newval ^= 0x8000;
					}
					dest[pos++] = (uint16)newval;
				}

				if (pos >= sz) {
					j++;
					pos = 0;
				}
			}
		} else {
			/* Data is 8-bit packed */
			struct bit_buffer bb;
			uint32 pos = 0;
			uint32 numbits = block.num_bits;
			uint32 j, oldval = 0;
			uint8 *ptable = mem+mempos;

			bb.count = 0;
			bb.buffer = 0;
			bb.src = mem+mempos+block.tt_entries;
			bb.end = mem+mempos+block.pk_size;

			for (j = 0; j < block.sub_blk; ) {
				uint32 sz = sub_block[j].unpk_size;
				uint8 *dest = buffer + sub_block[j].unpk_pos;
				uint32 newval = 0x100;
				uint32 d = get_bits(&bb,numbits+1);

				if (d >= cmd_8bits[numbits]) {
					uint32 nFetch = fetch_8bit[numbits];
					uint32 newbits = get_bits(&bb,nFetch) + ((d - cmd_8bits[numbits]) << nFetch);
					if (newbits != numbits) {
						numbits = newbits & 0x07;
					} else {
						if ((d = get_bits(&bb,3)) == 7)
						{
							if (get_bits(&bb,1)) break;
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
					if (block.flags & MMCMP_DELTA) {
						n += oldval;
						oldval = n;
					}
					dest[pos++] = (uint8)n;
				}

				if (pos >= sz) {
					j++;
					pos = 0;
				}
			}
		}

		free(sub_block);
	}

	free(table);
	fwrite(buffer, 1, h.filesize, out);
	free(mem);

	return 0;
}


int decrunch_mmcmp (FILE *f, FILE *fo)                          
{                                                          
	if (fo == NULL) 
		return -1; 

	mmcmp_unpack(f, fo);

	return 0;
}
