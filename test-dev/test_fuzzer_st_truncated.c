#include "test.h"

/* This input caused uninitialized reads in the ST MOD test function
 * due to a missing hio_read return value check.
 */

TEST(test_fuzzer_st_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_st_truncated.mod");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
