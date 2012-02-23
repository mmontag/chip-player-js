#include "test.h"
#include "convert.h"

TEST(test_convert_signal)
{
	uint8 buffer0[10] = { 0, 1, 2, 3,  4,  5,  6, -7,  8, -29 };
	uint8 buffer1[10] = { 0, 1, 2, 3,  4,  5,  6, -7,  8, -29 };
	uint8 conv_r0[10] = { 128, 129, 130, 131, 132, 133, 134, 121, 136, 99 };
	uint8 conv_r1[10] = { 0, 129, 2, 131, 4, 133, 6, 121, 8, 99 };

	convert_signal(10, 0, buffer0);
	fail_unless(memcmp(buffer0, conv_r0, 10) == 0,
				"Invalid 8-bit conversion");

	convert_signal(5, 1, buffer1);
	fail_unless(memcmp(buffer1, conv_r1, 10) == 0,
				"Invalid 16-bit conversion");
}
END_TEST
