#include "test.h"

/* This input caused invalid shifts in the ArcFS depacker due to no
 * bounds check on the compression bits value read from the format header.
 */

TEST(test_fuzzer_depack_arcfs_invalid_bits)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	ret = xmp_load_module(opaque, "data/f/depack_arcfs_invalid_bits");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
