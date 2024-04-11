#include "test.h"
#include "../src/loaders/loader.h"

TEST(test_sample_load_endian)
{
	static struct xmp_sample xxs;
	int8 conv_r0[10] = { 1, 0, 3, 2, 5, 4, -7,  6, -29,   8 };
	int8 conv_r1[10] = { 0, 1, 2, 3, 4, 5,  6, -7,   8, -29 };
	struct module_data m;

	memset(&m, 0, sizeof (struct module_data));

	xxs.len = 5;
	xxs.flg = XMP_SAMPLE_16BIT;

	/* Our input sample is big-endian */
	libxmp_load_sample(&m, NULL, SAMPLE_FLAG_NOLOAD | SAMPLE_FLAG_BIGEND, &xxs, conv_r0);

	if (is_big_endian()) {
		fail_unless(memcmp(xxs.data, conv_r0, 10) == 0,
					"Invalid conversion from big-endian");
	} else {
		fail_unless(memcmp(xxs.data, conv_r1, 10) == 0,
					"Invalid conversion from big-endian");
	}
	libxmp_free_sample(&xxs);

	/* Now the sample is little-endian */
	libxmp_load_sample(&m, NULL, SAMPLE_FLAG_NOLOAD, &xxs, conv_r0);
	if (is_big_endian()) {
		fail_unless(memcmp(xxs.data, conv_r1, 10) == 0,
					"Invalid conversion from little-endian");
	} else {
		fail_unless(memcmp(xxs.data, conv_r0, 10) == 0,
					"Invalid conversion from little-endian");
	}
	libxmp_free_sample(&xxs);
}
END_TEST
