#include "test.h"

/* This input caused heap corruption in the SQSH depacker
 * due to a missing bounds check for verbatim blocks.
 */

TEST(test_fuzzer_depack_sqsh_invalid_verbatim)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_sqsh_invalid_verbatim.xpk");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
