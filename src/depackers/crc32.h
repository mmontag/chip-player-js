#ifndef XMP_CRC_H
#define XMP_CRC_H

extern const uint32 crc_table[256];

void crc32_init(void);
uint32 crc32_1(const uint8 *, size_t, uint32);
uint32 crc32_2(const uint8 *, size_t, uint32);

#endif

