#include "test.h"
#include "convert.h"

TEST(test_convert_endian)
{
	uint8 buffer0[10] = { 0, 1, 2, 3, 4, 5,  6, -7,   8, -29 };
	uint8 conv_r0[10] = { 1, 0, 3, 2, 5, 4, -7,  6, -29,   8 };

	convert_endian(buffer0, 5);
	fail_unless(memcmp(buffer0, conv_r0, 10) == 0,
				"Invalid conversion");
}
END_TEST
