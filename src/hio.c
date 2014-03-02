/* Extended Module Player core player
 * Copyright (C) 1996-2014 Claudio Matsuoka and Hipolito Carraro Jr
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

#include <stdio.h>
#include <stdlib.h>
#ifndef LIBXMP_CORE_PLAYER
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <limits.h>
#include <errno.h>
#include "common.h"
#include "hio.h"

inline ptrdiff_t CAN_READ(HIO_HANDLE *h)
{
	if (h->size >= 0)
		return h->pos >= 0 ? h->size - h->pos : 0;

	return INT_MAX;
}

int8 hio_read8s(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read8s(h->f);
	} else {
		if (CAN_READ(h) >= 1) 
			return *(int8 *)(h->start + h->pos++);
		else
			return EOF;
	}
}

uint8 hio_read8(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read8(h->f);
	} else {
		if (CAN_READ(h) >= 1) 
			return *(uint8 *)(h->start + h->pos++);
		else
			return EOF;
	}
}

uint16 hio_read16l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read16l(h->f);
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 2) {
			uint16 n = readmem16l(h->start + h->pos);
			h->pos += 2;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

uint16 hio_read16b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read16b(h->f);
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 2) {
			uint16 n = readmem16b(h->start + h->pos);
			h->pos += 2;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

uint32 hio_read24l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read24l(h->f); 
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 3) {
			uint32 n = readmem24l(h->start + h->pos);
			h->pos += 3;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

uint32 hio_read24b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read24b(h->f);
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 3) {
			uint32 n = readmem24b(h->start + h->pos);
			h->pos += 3;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

uint32 hio_read32l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read32l(h->f);
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 4) {
			uint32 n = readmem32l(h->start + h->pos);
			h->pos += 4;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

uint32 hio_read32b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read32b(h->f);
	} else {
		ptrdiff_t can_read = CAN_READ(h);
		if (can_read >= 4) {
			uint32 n = readmem32b(h->start + h->pos);
			h->pos += 4;
			return n;
		} else {
			h->pos += can_read;
			return EOF;
		}
	}
}

size_t hio_read(void *buf, size_t size, size_t num, HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return fread(buf, size, num, h->f);
	} else {
 		size_t should_read = size * num;
 		ptrdiff_t can_read = CAN_READ(h);
 		if (can_read <= 0)
 			return 0;

 		if (should_read > can_read) {
 			should_read = can_read;
 		}

 		memcpy(buf, h->start + h->pos, should_read);
 		h->pos += should_read;

		return should_read / size;
	}
}

int hio_seek(HIO_HANDLE *h, long offset, int whence)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return fseek(h->f, offset, whence);
	} else {
		switch (whence) {
		default:
		case SEEK_SET:
			if (h->size >= 0 && (offset > h->size || offset < 0))
				return -1;
			h->pos = offset;
			return 0;
		case SEEK_CUR:
			if (h->size >= 0 && (offset > CAN_READ(h) || offset < -h->pos))
				return -1;
			h->pos += offset;
			return 0;
		case SEEK_END:
			if (h->size < 0)
				return -1;
			h->pos = h->size + offset;
			return 0;
		}
	}
}

long hio_tell(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE)
		return ftell(h->f);
	else
		return (long)h->pos;
}

int hio_eof(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return feof(h->f);
	} else {
		if (h->size <= 0)
			return 0;
		else
			return CAN_READ(h) <= 0;
	}
}

HIO_HANDLE *hio_open_file(void *path, char *mode)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		goto err;
	
	h->type = HIO_HANDLE_TYPE_FILE;
	h->f = fopen(path, mode);
	if (h->f == NULL)
		goto err2;

	return h;

    err2:
	free(h);
    err:
	return NULL;
}

HIO_HANDLE *hio_open_mem(void *path, long size)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		return NULL;
	
	h->type = HIO_HANDLE_TYPE_MEMORY;
	h->start = path;
	h->pos = 0;
	h->size = size;

	return h;
}

int hio_close(HIO_HANDLE *h)
{
	int ret;

	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		ret = fclose(h->f);
		free(h);
		return ret;
	} else {
		free(h);
		return 0;
	}
}

#ifndef LIBXMP_CORE_PLAYER

HIO_HANDLE *hio_open_fd(int fd, char *mode)
{
	HIO_HANDLE *h;
	
	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		return NULL;
	
	h->type = HIO_HANDLE_TYPE_FILE;
	h->f = fdopen(fd, mode);
	if (h->f == NULL) {
		free(h);
		return NULL;
	}

	return h;
}

int hio_stat(HIO_HANDLE *h, struct stat *st)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return fstat(fileno(h->f), st);
	} else {
		memset(st, 0, sizeof (struct stat));
		st->st_size = h->size;
		return 0;
	}
}

#endif

