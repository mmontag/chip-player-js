/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: readdata.c,v 1.2 2007-09-14 12:06:28 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xmpi.h"


inline uint8 read8(FILE *f)
{
	return (uint8)fgetc(f);
}

inline int8 read8s(FILE *f)
{
	return (int8)fgetc(f);
}

uint16 read16l(FILE *f)
{
	uint32 a, b;

	a = read8(f);
	b = read8(f);

	return (b << 8) | a;
}

uint16 read16b(FILE *f)
{
	uint32 a, b;

	a = read8(f);
	b = read8(f);

	return (a << 8) | b;
}

uint32 read24l(FILE *f)
{
	uint32 a, b, c;

	a = read8(f);
	b = read8(f);
	c = read8(f);

	return (c << 16) | (b << 8) | a;
}

uint32 read24b(FILE *f)
{
	uint32 a, b, c;

	a = read8(f);
	b = read8(f);
	c = read8(f);

	return (a << 16) | (b << 8) | c;
}

uint32 read32l(FILE *f)
{
	uint32 a, b, c, d;

	a = read8(f);
	b = read8(f);
	c = read8(f);
	d = read8(f);

	return (d << 24) | (c << 16) | (b << 8) | a;
}

uint32 read32b(FILE *f)
{
	uint32 a, b, c, d;

	a = read8(f);
	b = read8(f);
	c = read8(f);
	d = read8(f);

	return (a << 24) | (b << 16) | (c << 8) | d;
}



inline void write8(FILE *f, uint8 b)
{
	fputc(b, f);
}

void write16l(FILE *f, uint16 w)
{
	write8(f, w & 0x00ff);
	write8(f, (w & 0xff00) >> 8);
}

void write16b(FILE *f, uint16 w)
{
	write8(f, (w & 0xff00) >> 8);
	write8(f, w & 0x00ff);
}


