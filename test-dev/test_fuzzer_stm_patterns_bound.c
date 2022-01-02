#include "test.h"

/* This input caused slow loading and leaks in the STM loader due
 * to an invalid pattern count.
 */

TEST(test_fuzzer_stm_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_stm_patterns_bound.stm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
