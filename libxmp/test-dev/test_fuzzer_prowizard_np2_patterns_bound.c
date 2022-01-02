#include "test.h"

/* This input caused out-of-bounds writes in the NoisePacker 2
 * depacker due to not validating the number of patterns.
 */

TEST(test_fuzzer_prowizard_np2_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_np2_patterns_bound.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
