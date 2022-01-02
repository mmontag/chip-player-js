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
#include "miniz_zip.h"

static int test_zip(unsigned char *b)
{
	return b[0] == 'P' && b[1] == 'K' &&
		((b[2] == 3 && b[3] == 4) || (b[2] == '0' && b[3] == '0' &&
		b[4] == 'P' && b[5] == 'K' && b[6] == 3 && b[7] == 4));
}

#ifndef MINIZ_NO_ARCHIVE_APIS
static size_t mz_zip_file_read_func(void *pOpaque, mz_uint64 ofs, void *pBuf, size_t n)
{
	if (hio_seek((HIO_HANDLE *)pOpaque, (long)ofs, SEEK_SET))
		return 0;

	return hio_read(pBuf, 1, n, (HIO_HANDLE *)pOpaque);
}
#endif

static int decrunch_zip(HIO_HANDLE *in, void **out, long inlen, long *outlen)
{
#ifndef MINIZ_NO_ARCHIVE_APIS
	mz_zip_archive archive;
	char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
	mz_uint32 i;
	void *pBuf;
	size_t pSize;

	memset(&archive, 0, sizeof(archive));
	archive.m_pRead = mz_zip_file_read_func;
	archive.m_pIO_opaque = in;

	if (!mz_zip_reader_init(&archive, inlen, 0)) {
		D_(D_CRIT "Failed to open archive: %s", mz_zip_get_error_string(archive.m_last_error));
		return -1;
	}

	for (i = 0; i < archive.m_total_files; i++) {
		if (mz_zip_reader_get_filename(&archive, i, filename, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE) == 0) {
			D_(D_WARN "Could not get file name: %s", mz_zip_get_error_string(archive.m_last_error));
			continue;
		}
		if (mz_zip_reader_is_file_a_directory(&archive, i)) {
			D_(D_INFO "Skipping directory %s", filename);
			continue;
		}
		if (!mz_zip_reader_is_file_supported(&archive, i)) {
			D_(D_INFO "Skipping unsupported file %s", filename);
			continue;
		}
		if (libxmp_exclude_match(filename)) {
			D_(D_INFO "Skipping file %s", filename);
			continue;
		}

		pBuf = mz_zip_reader_extract_to_heap(&archive, i, &pSize, 0);
		if (!pBuf) {
			D_(D_CRIT "Failed to extract %s: %s", filename, mz_zip_get_error_string(archive.m_last_error));
			break;
		}

		mz_zip_reader_end(&archive);
		*out = pBuf;
		*outlen = pSize;
		return 0;
	}

	mz_zip_reader_end(&archive);
#endif
	return -1;
}

struct depacker libxmp_depacker_zip = {
	test_zip,
	NULL,
	decrunch_zip
};
