#include "test.h"

/* This input caused an out-of-bounds read in the SQSH depacker
 * due to an incorrect bounds check on an input buffer.
 */

TEST(test_fuzzer_depack_sqsh_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_sqsh_truncated.xpk");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
