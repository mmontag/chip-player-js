#include "test.h"

/* This input caused uninitialized reads due to not checking for
 * EOFs in the Slamtilt loader.
 */

TEST(test_fuzzer_stim_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_stim_truncated");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
