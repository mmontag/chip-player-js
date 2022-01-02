#include "test.h"

/* This input caused uninitialized reads in the Farandole Composer
 * loader due to several missing EOF checks.
 */

TEST(test_fuzzer_far_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_far_truncated.far");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
