#include "test.h"
#include "../src/loaders/loader.h"

struct xmp_sample xxs;

TEST(test_sample_load_endian)
{
	uint8 buffer0[10] = { 0, 1, 2, 3, 4, 5,  6, -7,   8, -29 };
	uint8 conv_r0[10] = { 1, 0, 3, 2, 5, 4, -7,  6, -29,   8 };

	xxs.len = 10;
	xxs.flg = XMP_SAMPLE_16BIT;
	load_sample(NULL, SAMPLE_FLAG_NOLOAD | SAMPLE_FLAG_BIGEND, &xxs, buffer0);
	fail_unless(memcmp(xxs.data, conv_r0, 10) == 0,
					"Invalid conversion");
}
END_TEST
