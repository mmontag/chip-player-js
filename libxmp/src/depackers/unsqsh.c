/*
 * XPK-SQSH depacker
 * Algorithm from the portable decruncher by Bert Jahn (24.12.97)
 * Checksum added by Sipos Attila <h430827@stud.u-szeged.hu>
 * Rewritten for libxmp by Claudio Matsuoka
 *
 * Copyright (C) 2013 Claudio Matsuoka
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

struct io {
	uint8 *src;
	uint8 *dest;
	int offs;
	int srclen;
};

static uint8 ctable[] = {
	2, 3, 4, 5, 6, 7, 8, 0,
	3, 2, 4, 5, 6, 7, 8, 0,
	4, 3, 5, 2, 6, 7, 8, 0,
	5, 4, 6, 2, 3, 7, 8, 0,
	6, 5, 7, 2, 3, 4, 8, 0,
	7, 6, 8, 2, 3, 4, 5, 0,
	8, 7, 6, 2, 3, 4, 5, 0
};

static uint16 xchecksum(uint8 *ptr, uint32 count)
{
	uint32 sum = 0;

	while (count-- > 0) {
		sum ^= readmem32b(ptr);
		ptr += 4;
	}

	return (uint16) (sum ^ (sum >> 16));
}

static int has_bits(struct io *io, int count)
{
	return (count <= io->srclen - io->offs);
}

static int get_bits(struct io *io, int count)
{
	int r;

	if (!has_bits(io, count)) {
		return -1;
	}

	r = readmem24b(io->src + (io->offs >> 3));

	r <<= io->offs % 8;
	r &= 0xffffff;
	r >>= 24 - count;
	io->offs += count;

	return r;
}

static int get_bits_final(struct io *io, int count)
{
	/* Note: has_bits check should be done separately since
	 * this can return negative values.
	 */
	int r = readmem24b(io->src + (io->offs >> 3));

	r <<= (io->offs % 8) + 8;
	r >>= 32 - count;
	io->offs += count;

	return r;
}

static int copy_data(struct io *io, int d1, int *data, uint8 *dest_start, uint8 *dest_end)
{
	uint8 *copy_src;
	int r, dest_offset, count, copy_len;

	if (get_bits(io, 1) == 0) {
		copy_len = get_bits(io, 1) + 2;
	} else if (get_bits(io, 1) == 0) {
		copy_len = get_bits(io, 1) + 4;
	} else if (get_bits(io, 1) == 0) {
		copy_len = get_bits(io, 1) + 6;
	} else if (get_bits(io, 1) == 0) {
		copy_len = get_bits(io, 3) + 8;
	} else {
		copy_len = get_bits(io, 5) + 16;
	}

	r = get_bits(io, 1);
	if (copy_len < 0 || r < 0) {
		return -1;
	}
	if (r == 0) {
		r = get_bits(io, 1);
		if (r < 0) {
			return -1;
		}
		if (r == 0) {
			count = 8;
			dest_offset = 0;
		} else {
			count = 14;
			dest_offset = -0x1100;
		}
	} else {
		count = 12;
		dest_offset = -0x100;
	}

	copy_len -= 3;

	if (copy_len >= 0) {
		if (copy_len != 0) {
			d1--;
		}
		d1--;
		if (d1 < 0) {
			d1 = 0;
		}
	}

	copy_len += 2;

	r = get_bits(io, count);
	if (r < 0) {
		return -1;
	}
	copy_src = io->dest + dest_offset - r - 1;

	/* Sanity check */
	if (copy_src < dest_start || copy_src + copy_len >= dest_end) {
		return -1;
	}

	do {
		//printf("dest=%p src=%p end=%p\n", io->dest, copy_src, dest_end);
		*io->dest++ = *copy_src++;
	} while (copy_len--);

	*data = *(--copy_src);

	return d1;
}

static int unsqsh_block(struct io *io, uint8 *dest_start, uint8 *dest_end)
{
	int r, d1, d2, data, unpack_len, count, old_count;

	d1 = d2 = data = old_count = 0;
	io->offs = 0;

	data = *(io->src++);
	*(io->dest++) = data;

	do {
		r = get_bits(io, 1);
		if (r < 0)
			return -1;

		if (d1 < 8) {
			if (r) {
				d1 = copy_data(io, d1, &data, dest_start, dest_end);
				if (d1 < 0)
					return -1;
				d2 -= d2 >> 3;
				continue;
			}
			unpack_len = 0;
			count = 8;
		} else {
			if (r) {
				count = 8;
				if (count == old_count) {
					if (d2 >= 20) {
						unpack_len = 1;
						d2 += 8;
					} else {
						unpack_len = 0;
					}
				} else {
					count = old_count;
					unpack_len = 4;
					d2 += 8;
				}
			} else {
				r = get_bits(io, 1);
				if (r < 0)
					return -1;

				if (r == 0) {
					d1 = copy_data(io, d1, &data, dest_start, dest_end);
					if (d1 < 0)
						return -1;
					d2 -= d2 >> 3;
					continue;
				}

				r = get_bits(io, 1);
				if (r < 0)
					return -1;

				if (r == 0) {
					count = 2;
				} else {
					r = get_bits(io, 1);
					if (r < 0)
						return -1;

					if (r) {
						io->offs--;
						count = get_bits(io, 3);
						if (count < 0)
							return -1;
					} else {
						count = 3;
					}
				}

				count = ctable[8 * old_count + count - 17];
				if (count != 8) {
					unpack_len = 4;
					d2 += 8;
				} else {
					if (d2 >= 20) {
						unpack_len = 1;
						d2 += 8;
					} else {
						unpack_len = 0;
					}
				}
			}
		}

		if (!has_bits(io, count * (unpack_len + 2))) {
			return -1;
		}

		do {
			data -= get_bits_final(io, count);
			*io->dest++ = data;
		} while (unpack_len--);

		if (d1 != 31) {
			d1++;
		}

		old_count = count;

		d2 -= d2 >> 3;

	} while (io->dest < dest_end);

	return 0;
}

static int unsqsh(uint8 *src, int srclen, uint8 *dest, int destlen)
{
	int len = destlen;
	int decrunched = 0;
	int type;
	int sum, packed_size, unpacked_size;
	int lchk;
	uint8 *c, *dest_start, *dest_end;
	uint8 bc[3];
	struct io io;

	io.src = src;
	io.dest = dest;

	dest_start = io.dest;

	c = src + 20;

	while (len) {
		/* Sanity check */
		if (c + 8 > src + srclen) {
			return -1;
		}

		type = *c++;
		c++;			/* hchk */

		sum = readmem16b(c);
		c += 2;			/* checksum */

		packed_size = readmem16b(c);	/* packed */
		c += 2;

		unpacked_size = readmem16b(c);	/* unpacked */
		c += 2;

		/* Sanity check */
		if (packed_size <= 0 || unpacked_size <= 0) {
			return -1;
		}

		if (c + packed_size + 3 > src + srclen) {
			return -1;
		}

		io.src = c + 2;
		io.srclen = packed_size << 3;
		memcpy(bc, c + packed_size, 3);
		memset(c + packed_size, 0, 3);
		lchk = xchecksum(c, (packed_size + 3) >> 2);
		memcpy(c + packed_size, bc, 3);

		if (lchk != sum) {
			return decrunched;
		}

		if (type == 0) {
			/* verbatim block */
			decrunched += packed_size;
			if (decrunched > destlen) {
				return -1;
			}
			memcpy(io.dest, c, packed_size);
			io.dest += packed_size;
			c += packed_size;
			len -= packed_size;
			continue;
		}

		if (type != 1) {
			/* unknown type */
			return decrunched;
		}

		len -= unpacked_size;
		decrunched += unpacked_size;

		/* Sanity check */
		if (decrunched > destlen) {
			return -1;
		}

		packed_size = (packed_size + 3) & 0xfffc;
		c += packed_size;
		dest_end = io.dest + unpacked_size;

		if (unsqsh_block(&io, dest_start, dest_end) < 0) {
			return -1;
		}

		io.dest = dest_end;
	}

	return decrunched;

}

static int test_sqsh(unsigned char *b)
{
	return memcmp(b, "XPKF", 4) == 0 && memcmp(b + 8, "SQSH", 4) == 0;
}

static int decrunch_sqsh(HIO_HANDLE * f, void ** outbuf, long inlen, long * outlen)
{
	unsigned char *src, *dest;
	int srclen, destlen;

	if (hio_read32b(f) != 0x58504b46)	/* XPKF */
		goto err;

	srclen = hio_read32b(f);

	/* Sanity check */
	if (srclen <= 8 || srclen > 0x100000)
		goto err;

	if (hio_read32b(f) != 0x53515348)	/* SQSH */
		goto err;

	destlen = hio_read32b(f);
	if (destlen < 0 || destlen > 0x100000)
		goto err;

	if ((src = (unsigned char *)calloc(1, srclen + 3)) == NULL)
		goto err;

	if ((dest = (unsigned char *)malloc(destlen + 100)) == NULL)
		goto err2;

	if (hio_read(src, srclen - 8, 1, f) != 1)
		goto err3;

	if (unsqsh(src, srclen, dest, destlen) != destlen)
		goto err3;

	free(src);

	*outbuf = dest;
	*outlen = destlen;

	return 0;

    err3:
	free(dest);
    err2:
	free(src);
    err:
	return -1;
}

struct depacker libxmp_depacker_sqsh = {
	test_sqsh,
	NULL,
	decrunch_sqsh
};
