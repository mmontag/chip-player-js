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

int8 hread_8s(HANDLE *h)
{
	return read8s(h->f);
}

uint8 hread_8(HANDLE *h)
{
	return read8(h->f);
}

uint16 hread_16l(HANDLE *h)
{
	return read16l(h->f);
}

uint16 hread_16b(HANDLE *h)
{
	return read16b(h->f);
}

uint32 hread_24l(HANDLE *h)
{
	return read24l(h->f);
}

uint32 hread_24b(HANDLE *h)
{
	return read24b(h->f);
}

uint32 hread_32l(HANDLE *h)
{
	return read32l(h->f);
}

uint32 hread_32b(HANDLE *h)
{
	return read32b(h->f);
}

int hread(void *buf, int size, int num, HANDLE *h)
{
	return fread(buf, size, num, h->f);
}

int hseek(HANDLE *h, long offset, int whence)
{
	return fseek(h->f, offset, whence);
}

long htell(HANDLE *h)
{
	return ftell(h->f);
}

int heof(HANDLE *h)
{
	return feof(h->f);
}

HANDLE *hopen(void *path, int type)
{
	HANDLE *h;

	h = (HANDLE *)malloc(sizeof (HANDLE));
	if (h == NULL)
		return NULL;
	
	h->type = type;
	h->f = fopen(path, "rb");

	return h;
}

int hclose(HANDLE *h)
{
	int ret;

	ret = fclose(h->f);
	free(h);

	return ret;
}

