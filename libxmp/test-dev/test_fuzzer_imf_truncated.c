#include "test.h"

/* This input caused uninitialized reads in the IMF loader
 * due to a missing EOF check on the order list.
 */

TEST(test_fuzzer_imf_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_imf_truncated.imf");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
