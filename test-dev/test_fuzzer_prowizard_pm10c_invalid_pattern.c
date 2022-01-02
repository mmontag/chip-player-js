#include "test.h"

/* This input caused an out-of-bounds read in the Promizer 1.0c
 * depacker due to failure to validate sample and finetune values.
 */

TEST(test_fuzzer_prowizard_pm10c_invalid_pattern)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pm10c_invalid_pattern.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
