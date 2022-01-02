#include "test.h"

/* This input caused an out-of-bounds read in the SQSH depacker
 * due to missing bounds checks when parsing the input bitstream.
 */

TEST(test_fuzzer_depack_sqsh_truncated2)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_sqsh_truncated2.xpk");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
