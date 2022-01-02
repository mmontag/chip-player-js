#include "test.h"

/* This input caused signed overflows due to not properly bounds checking
 * NoiseRunner modules' sample offsets.
 */

TEST(test_fuzzer_prowizard_noiserun_invalid_sample)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_noiserun_invalid_sample");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
