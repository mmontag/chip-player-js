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

#include "../common.h"
#include "depacker.h"
#include "miniz.h"

static int test_muse(unsigned char *b)
{
	if (memcmp(b, "MUSE", 4) == 0) {
		uint32 r = readmem32b(b + 4);
		/* MOD2J2B uses 0xdeadbabe */
		if (r == 0xdeadbeaf || r == 0xdeadbabe) {
			return 1;
		}
	}

	return 0;
}

static int decrunch_muse(HIO_HANDLE *f, void **out, long inlen, long *outlen)
{
	size_t in_buf_size = inlen - 24;
	void *pCmp_data, *pOut_buf;
	size_t pOut_len;

	if (hio_seek(f, 24, SEEK_SET) < 0) {
		D_(D_CRIT "hio_seek() failed");
		return -1;
	}

	pCmp_data = (uint8 *)malloc(in_buf_size);
	if (!pCmp_data) {
		D_(D_CRIT "Out of memory");
		return -1;
	}

	if (hio_read(pCmp_data, 1, in_buf_size, f) != in_buf_size) {
		D_(D_CRIT "Failed reading input file");
		free(pCmp_data);
		return -1;
	}

	pOut_buf = tinfl_decompress_mem_to_heap(pCmp_data, in_buf_size, &pOut_len, TINFL_FLAG_PARSE_ZLIB_HEADER);
	if (!pOut_buf) {
		D_(D_CRIT "tinfl_decompress_mem_to_heap() failed");
		free(pCmp_data);
		return -1;
	}

	free(pCmp_data);

	*out = pOut_buf;
	*outlen = pOut_len;

	return 0;
}

struct depacker libxmp_depacker_muse = {
	test_muse,
	NULL,
	decrunch_muse
};
