#ifndef LIBXMP_CRC_H
#define LIBXMP_CRC_H

extern uint32 libxmp_crc32_table_A[256];
extern uint32 libxmp_crc32_table_B[256];

void	libxmp_crc32_init_A	(void);
uint32	libxmp_crc32_A1		(const uint8 *, size_t, uint32);
uint32	libxmp_crc32_A2		(const uint8 *, size_t, uint32);
void	libxmp_crc32_init_B	(void);

#endif

