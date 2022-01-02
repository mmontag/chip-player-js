#include "test.h"

/* This input caused uninitialized reads in the ProPacker 2.1 depacker
 * due to a missing return value check on reading the reference table.
 */

TEST(test_fuzzer_prowizard_pp21_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pp21_truncated");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
