#include "test.h"

/* This input caused uninitialized reads in the bzip2 depacker
 * due to a faulty sanity check on group selectors.
 */

TEST(test_fuzzer_depack_bz2_invalid_selector)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_bz2_invalid_selector.bz2");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
