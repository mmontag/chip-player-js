/* ArcFS depacker for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * Based on the nomarch .arc depacker from nomarch
 * Copyright (C) 2001-2006 Russell Marks
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "../common.h"
#include "depacker.h"
#include "readlzw.h"


struct archived_file_header_tag {
	unsigned char method;
	unsigned char bits;
	char name[13];
	unsigned int compressed_size;
	unsigned int date, time, crc;
	unsigned int orig_size;
	unsigned int offset;
};


static int read_file_header(HIO_HANDLE *in, struct archived_file_header_tag *hdrp)
{
	int hlen, start /*, ver*/;
	int i;

	if (hio_seek(in, 8, SEEK_CUR) < 0)		/* skip magic */
		return -1;
	hlen = hio_read32l(in) / 36;
	if (hio_error(in) != 0) return -1;
	if (hlen < 1) return -1;
	start = hio_read32l(in);
	if (hio_error(in) != 0) return -1;
	/*ver =*/ hio_read32l(in);
	if (hio_error(in) != 0) return -1;

	hio_read32l(in);
	if (hio_error(in) != 0) return -1;
	/*ver =*/ hio_read32l(in);
	if (hio_error(in) != 0) return -1;

	if (hio_seek(in, 68, SEEK_CUR) < 0)	/* reserved */
		return -1;

	for (i = 0; i < hlen; i++) {
		int x = hio_read8(in);
		if (hio_error(in) != 0) return -1;

		if (x == 0)			/* end? */
			break;

		hdrp->method = x & 0x7f;
		if (hio_read(hdrp->name, 1, 11, in) != 11) {
			return -1;
		}
		hdrp->name[12] = 0;
		hdrp->orig_size = hio_read32l(in);
		if (hio_error(in) != 0) return -1;
		hio_read32l(in);
		if (hio_error(in) != 0) return -1;
		hio_read32l(in);
		if (hio_error(in) != 0) return -1;
		x = hio_read32l(in);
		if (hio_error(in) != 0) return -1;
		hdrp->compressed_size = hio_read32l(in);
		if (hio_error(in) != 0) return -1;
		hdrp->offset = hio_read32l(in);
		if (hio_error(in) != 0) return -1;

		if (x == 1)			/* deleted */
			continue;

		if (hdrp->offset & 0x80000000)		/* directory */
			continue;

		hdrp->crc = x >> 16;
		hdrp->bits = (x & 0xff00) >> 8;
		hdrp->offset &= 0x7fffffff;
		hdrp->offset += start;

		/* Max allowed compression bits value is 16 for method FFh. */
		if (hdrp->method > 2 && hdrp->bits > 16)
			return -1;

		return 0;
	}

	/* no usable files */
	return -1;
}

/* read file data, assuming header has just been read from in
 * and hdrp's data matches it. Caller is responsible for freeing
 * the memory allocated.
 * Returns NULL for file I/O error only; OOM is fatal (doesn't return).
 */
static unsigned char *read_file_data(HIO_HANDLE *in, long inlen,
				     struct archived_file_header_tag *hdrp)
{
	unsigned char *data;
	int siz = hdrp->compressed_size;

	/* Precheck: if the file can't hold this size, don't bother. */
	if (siz <= 0 || inlen < siz)
		return NULL;

	data = (unsigned char *) malloc(siz);
	if (data == NULL) {
		goto err;
	}
	if (hio_seek(in, hdrp->offset, SEEK_SET) < 0) {
		goto err2;
	}
	if (hio_read(data, 1, siz, in) != siz) {
		goto err2;
	}

	return data;

    err2:
	free(data);
    err:
	return NULL;
}

static int arcfs_extract(HIO_HANDLE *in, void **out, long inlen, long *outlen)
{
	struct archived_file_header_tag hdr;
	unsigned char *data, *orig_data;

	if (read_file_header(in, &hdr) < 0)
		return -1;

	if (hdr.method == 0)
		return -1;

	/* error reading data (hit EOF) */
	if ((data = read_file_data(in, inlen, &hdr)) == NULL)
		return -1;

	orig_data = NULL;

	/* FWIW, most common types are (by far) 8/9 and 2.
	 * (127 is the most common in Spark archives, but only those.)
	 * 3 and 4 crop up occasionally. 5 and 6 are very, very rare.
	 * And I don't think I've seen a *single* file with 1 or 7 yet.
	 */
	switch (hdr.method) {
	case 2:		/* no compression */
		if (hdr.orig_size != hdr.compressed_size) {
			free(data);
			return -1;
		}
		orig_data = data;
		break;

	case 8:		/* "Crunched" [sic]
			 * (RLE+9-to-12-bit dynamic LZW, a *bit* like GIF) */
		orig_data = libxmp_convert_lzw_dynamic(data, hdr.bits, 1,
					hdr.compressed_size, hdr.orig_size, 0);
		break;

	case 9:		/* "Squashed" (9-to-13-bit, no RLE) */
		orig_data = libxmp_convert_lzw_dynamic(data, hdr.bits, 0,
					hdr.compressed_size, hdr.orig_size, 0);
		break;

	case 127:	/* "Compress" (9-to-16-bit, no RLE) ("Spark" only) */
		orig_data = libxmp_convert_lzw_dynamic(data, hdr.bits, 0,
					hdr.compressed_size, hdr.orig_size, 0);
		break;

	default:
		free(data);
		return -1;
	}

	if (orig_data == NULL) {
		free(data);
		return -1;
	}

	if (orig_data != data)	/* don't free uncompressed stuff twice :-) */
		free(data);

	*out = orig_data;
	*outlen = hdr.orig_size;

	return 0;
}

static int test_arcfs(unsigned char *b)
{
	return !memcmp(b, "Archive\0", 8);
}

static int decrunch_arcfs(HIO_HANDLE *f, void **out, long inlen, long *outlen)
{
	return arcfs_extract(f, out, inlen, outlen);
}

struct depacker libxmp_depacker_arcfs = {
	test_arcfs,
	NULL,
	decrunch_arcfs
};
