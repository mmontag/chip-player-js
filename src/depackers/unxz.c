/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
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

#include "depacker.h"
#include "xz.h"
#include "crc32.h"

#define XZ_MAX_OUTPUT	(512 << 20)
#define XZ_MAX_DICT	(16 << 20)
#define XZ_BUFFER_SIZE	4096

static const uint8 XZ_MAGIC[] = { 0xfd, '7', 'z', 'X', 'Z', 0x00 };

static int test_xz(unsigned char *b)
{
	return !memcmp(b, XZ_MAGIC, sizeof(XZ_MAGIC));
}

static int decrunch_xz(HIO_HANDLE *in, void **out, long inlen, long *outlen)
{
	struct xz_dec *xz;
	struct xz_buf buf;
	enum xz_ret ret = XZ_OK;
	uint8 *inbuf = NULL;
	uint8 *tmp;

	libxmp_crc32_init_A();

	xz = xz_dec_init(XZ_DYNALLOC, XZ_MAX_DICT);
	if (xz == NULL)
		return -1;

	if ((buf.out = (uint8 *) malloc(XZ_BUFFER_SIZE)) == NULL)
		goto err;
	if ((inbuf = (uint8 *) malloc(XZ_BUFFER_SIZE)) == NULL)
		goto err;

	buf.in = inbuf;
	buf.in_pos = 0;
	buf.in_size = 0;
	buf.out_pos = 0;
	buf.out_size = XZ_BUFFER_SIZE;

	while (ret != XZ_STREAM_END) {
		if (buf.out_pos == buf.out_size) {
			/* Allocate more output space. */
			buf.out_size <<= 1;
			if (buf.out_size > XZ_MAX_OUTPUT)
				goto err;

			if ((tmp = (uint8 *) realloc(buf.out, buf.out_size)) == NULL)
				goto err;
			buf.out = tmp;
		}
		else if (buf.in_pos == buf.in_size) {
			/* Read input. */
			buf.in_pos = 0;
			buf.in_size = hio_read(inbuf, 1, XZ_BUFFER_SIZE, in);
			if (buf.in_size == 0)
				goto err;
		}

		ret = xz_dec_run(xz, &buf);
		if (ret != XZ_OK && ret != XZ_STREAM_END && ret != XZ_UNSUPPORTED_CHECK)
			goto err;
	}

	xz_dec_end(xz);

	if ((tmp = (uint8 *) realloc(buf.out, buf.out_pos)) != NULL)
		buf.out = tmp;

	*out = buf.out;
	*outlen = buf.out_pos;

	free(inbuf);
	return 0;

    err:
	xz_dec_end(xz);
	free(buf.out);
	free(inbuf);
	return -1;
}

struct depacker libxmp_depacker_xz = {
	test_xz,
	NULL,
	decrunch_xz
};
