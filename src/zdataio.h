#ifndef LIBXMP_ZDATAIO_H
#define LIBXMP_ZDATAIO_H

#include "common.h"
#include "zipio.h"


static inline uint8 zread8(ZFILE *z)
{
	return (uint8)Zgetc(z);
}

static inline int8 zread8s(ZFILE *z)
{
	return (int8)Zgetc(z);
}

static inline uint16 zread16l(ZFILE *z)
{
	uint32 a, b;

	a = zread8(z);
	b = zread8(z);

	return (b << 8) | a;
}

static inline uint16 zread16b(ZFILE *z)
{
	uint32 a, b;

	a = zread8(z);
	b = zread8(z);

	return (a << 8) | b;
}

static inline uint32 zread24l(ZFILE *z)
{
	uint32 a, b, c;

	a = zread8(z);
	b = zread8(z);
	c = zread8(z);

	return (c << 16) | (b << 8) | a;
}

static inline uint32 zread24b(ZFILE *z)
{
	uint32 a, b, c;

	a = zread8(z);
	b = zread8(z);
	c = zread8(z);

	return (a << 16) | (b << 8) | c;
}

static inline uint32 zread32l(ZFILE *z)
{
	uint32 a, b, c, d;

	a = zread8(z);
	b = zread8(z);
	c = zread8(z);
	d = zread8(z);

	return (d << 24) | (c << 16) | (b << 8) | a;
}

static inline uint32 zread32b(ZFILE *z)
{
	uint32 a, b, c, d;

	a = zread8(z);
	b = zread8(z);
	c = zread8(z);
	d = zread8(z);

	return (a << 24) | (b << 16) | (c << 8) | d;
}

#endif
