#include "test.h"

/* This input caused uninitialized reads in the ST 2.6/Ice loader
 * due to a missing EOF check.
 */

TEST(test_fuzzer_ice_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ice_truncated.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
