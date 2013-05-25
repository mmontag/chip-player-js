/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"
#include "hio.h"

int8 hio_read8s(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE)
		return read8s(h->f);
	else
		return *(int8 *)h->p++;
}

uint8 hio_read8(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE)
		return read8(h->f);
	else
		return *(uint8 *)h->p++;
}

uint16 hio_read16l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read16l(h->f);
	} else {
		uint16 n = readmem16l(h->p);
		h->p += 2;
		return n;
	}
}

uint16 hio_read16b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read16b(h->f);
	} else {
		uint16 n = readmem16b(h->p);
		h->p += 2;
		return n;
	}
}

uint32 hio_read24l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read24l(h->f); 
	} else {
		uint32 n = readmem24l(h->p);
		h->p += 3;
		return n;
	}
}

uint32 hio_read24b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read24b(h->f);
	} else {
		uint32 n = readmem24b(h->p);
		h->p += 3;
		return n;
	}
}

uint32 hio_read32l(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read32l(h->f);
	} else {
		uint32 n = readmem32l(h->p);
		h->p += 4;
		return n;
	}
}

uint32 hio_read32b(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return read32b(h->f);
	} else {
		uint32 n = readmem32b(h->p);
		h->p += 4;
		return n;
	}
}

int hio_read(void *buf, int size, int num, HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return fread(buf, size, num, h->f);
	} else {
		size *= num;
		memcpy(buf, h->p, size);
		h->p += size;
		return num;
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
			h->p = h->start + offset;
			return 0;
		case SEEK_CUR:
			h->p += offset;
			return 0;
		case SEEK_END:
			errno = EINVAL;
			return -1;
		}
	}
}

long hio_tell(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE)
		return ftell(h->f);
	else
		return (long)h->p - (long)h->start;
}

int hio_eof(HIO_HANDLE *h)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE)
		return feof(h->f);
	else
		return 0;
}

HIO_HANDLE *hio_open(void *path, int type)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		return NULL;
	
	if ((h->type = type) == HIO_HANDLE_TYPE_FILE) {
		h->f = fopen(path, "rb");
	} else {
		h->p = h->start = path;
	}

	return h;
}

HIO_HANDLE *hio_fdopen(int fd, char *mode)
{
	HIO_HANDLE *h;
	
	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		return NULL;
	
	h->type = HIO_HANDLE_TYPE_FILE;
	h->f = fdopen(fd, mode);

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

int hio_stat(HIO_HANDLE *h, struct stat *st)
{
	if (HIO_HANDLE_TYPE(h) == HIO_HANDLE_TYPE_FILE) {
		return fstat(fileno(h->f), st);
	} else {
		errno = EINVAL;
		return -1;
	}
}
