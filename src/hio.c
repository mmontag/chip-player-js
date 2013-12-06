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

inline ptrdiff_t CAN_READ(HIO_HANDLE *h)
{
  return h->pos >= 0 ? h->size - h->pos : 0;
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
			if (offset > h->size || offset < 0)
				return -1;
			h->pos = offset;
			return 0;
		case SEEK_CUR:
			if (offset > CAN_READ(h) || offset < -h->pos)
				return -1;
			h->pos += offset;
			return 0;
		case SEEK_END:
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
		memset(st, 0, sizeof (struct stat));
		st->st_size = h->size;
		return 0;
	}
}
