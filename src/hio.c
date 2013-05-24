/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "hio.h"

int8 hio_read8s(HIO_HANDLE *h)
{
	return read8s(h->f);
}

uint8 hio_read8(HIO_HANDLE *h)
{
	return read8(h->f);
}

uint16 hio_read16l(HIO_HANDLE *h)
{
	return read16l(h->f);
}

uint16 hio_read16b(HIO_HANDLE *h)
{
	return read16b(h->f);
}

uint32 hio_read24l(HIO_HANDLE *h)
{
	return read24l(h->f);
}

uint32 hio_read24b(HIO_HANDLE *h)
{
	return read24b(h->f);
}

uint32 hio_read32l(HIO_HANDLE *h)
{
	return read32l(h->f);
}

uint32 hio_read32b(HIO_HANDLE *h)
{
	return read32b(h->f);
}

int hio_read(void *buf, int size, int num, HIO_HANDLE *h)
{
	return fread(buf, size, num, h->f);
}

int hio_seek(HIO_HANDLE *h, long offset, int whence)
{
	return fseek(h->f, offset, whence);
}

long hio_tell(HIO_HANDLE *h)
{
	return ftell(h->f);
}

int hio_eof(HIO_HANDLE *h)
{
	return feof(h->f);
}

HIO_HANDLE *hio_open(void *path, int type)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *)malloc(sizeof (HIO_HANDLE));
	if (h == NULL)
		return NULL;
	
	h->type = type;
	h->f = fopen(path, "rb");

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

	ret = fclose(h->f);
	free(h);

	return ret;
}

