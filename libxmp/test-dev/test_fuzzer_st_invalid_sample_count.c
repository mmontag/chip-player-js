#include "test.h"

/* This input caused an out-of-bounds read in the
 * ST loader due to an invalid instrument count.
 */

TEST(test_fuzzer_st_invalid_sample_count)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_st_invalid_sample_count.mod.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
