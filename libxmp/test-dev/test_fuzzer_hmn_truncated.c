#include "test.h"

/* This input caused uninitialized reads in the His Master's Noise
 * loader due to missing EOF checks when loading patterns.
 */

TEST(test_fuzzer_hmn_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_hmn_truncated.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
