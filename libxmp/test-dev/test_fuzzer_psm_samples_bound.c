#include "test.h"


TEST(test_fuzzer_psm_samples_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input crashed the PSM loader due to containing a samples
	 * count over 64, which overflowed the sample offsets array.
	 */
	ret = xmp_load_module(opaque, "data/f/load_psm_samples_bound.psm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	/* The maximum sample length supported in PS16 PSMs is 64k.
	 */
	ret = xmp_load_module(opaque, "data/f/load_psm_samples_bound2.psm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (2)");

	xmp_free_context(opaque);
}
END_TEST
