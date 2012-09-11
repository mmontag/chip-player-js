/*
 * CRC32 using the polynomial from IEEE-802.3
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <http://7-zip.org/>
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

/*
 * This is not the fastest implementation, but it is pretty compact.
 * The fastest versions of xz_crc32() on modern CPUs without hardware
 * accelerated CRC instruction are 3-5 times as fast as this version,
 * but they are bigger and use more memory for the lookup table.
 */

#include "xz_private.h"

/*
 * STATIC_RW_DATA is used in the pre-boot environment on some architectures.
 * See <linux/decompress/mm.h> for details.
 */
#ifndef STATIC_RW_DATA
#	define STATIC_RW_DATA static
#endif

STATIC_RW_DATA uint32 xz_crc32_table[256];

XZ_EXTERN void xz_crc32_init(void)
{
	static int once = 0;
	const uint32 poly = 0xEDB88320;
	uint32 i;
	uint32 j;
	uint32 r;

	if (once)
		return;
	once = 1;
	
	for (i = 0; i < 256; ++i) {
		r = i;
		for (j = 0; j < 8; ++j)
			r = (r >> 1) ^ (poly & ~((r & 1) - 1));

		xz_crc32_table[i] = r;
	}

	return;
}

XZ_EXTERN uint32 xz_crc32(const uint8 *buf, size_t size, uint32 crc)
{
	crc = ~crc;

	while (size != 0) {
		crc = xz_crc32_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
