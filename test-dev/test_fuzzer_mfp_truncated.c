#include "test.h"

/* This input caused uninitialized reads in the MFP loader
 * due to missing bounds checks when unpacking patterns.
 */

TEST(test_fuzzer_mfp_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mfp_truncated.mfp");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
