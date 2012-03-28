#include "test.h"
#include "convert.h"

TEST(test_convert_signal)
{
	uint8  buffer0[10] = { 0, 1, 2, 3,  4,  5,  6, -7,  8, -29 };
	uint16 buffer1[10] = { 0, 1, 2, 3,  4,  5,  6, -7,  8, -29 };

	uint8  conv_r0[10] = {
		128, 129, 130, 131, 132, 133, 134, 121, 136, 99
	};
	uint16 conv_r1[10] = {
		32768, 32769, 32770, 32771, 32772, 
		32773, 32774, 32761, 32776, 32739
	};

	convert_signal(buffer0, 10, 0);
	fail_unless(memcmp(buffer0, conv_r0, 10) == 0,
				"Invalid 8-bit conversion");

	convert_signal((uint8 *)buffer1, 10, 1);
	fail_unless(memcmp(buffer1, conv_r1, 20) == 0,
				"Invalid 16-bit conversion");
}
END_TEST
