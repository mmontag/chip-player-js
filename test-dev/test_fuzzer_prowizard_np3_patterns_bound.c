#include "test.h"

/* This input caused an out-of-bounds write in the NoisePacker 3
 * depacker due to a missing pattern count bounds check.
 */

TEST(test_fuzzer_prowizard_np3_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_np3_patterns_bound.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
