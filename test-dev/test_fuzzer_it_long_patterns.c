#include "test.h"

/* This input caused slow loads and high RAM usage due to attempting
 * to allocate a large number of patterns of excessive length.
 */

TEST(test_fuzzer_it_long_patterns)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_it_long_patterns.it.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
