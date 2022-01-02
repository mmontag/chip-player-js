#include "test.h"

/* This input caused uninitialized memory reads in the MDL loader
 * due to not clearing extra bytes allocated for the sample unpackers.
 */

TEST(test_fuzzer_mdl_umr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_umr.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
