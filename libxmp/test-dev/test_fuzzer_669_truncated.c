#include "test.h"

/* This input caused uninitialized reads in the 669 loader
 * due to missing EOF checks when unpacking patterns.
 */

TEST(test_fuzzer_669_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_669_truncated.669");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
