#include "test.h"

/* This input caused hangs in the IT loader due to missing EOF
 * checks during the pattern scan and pattern loading loops.
 */

TEST(test_fuzzer_it_truncated_pattern)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_it_truncated_pattern.it");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
