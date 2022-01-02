#include "test.h"

/* This input caused an out-of-bounds read in the LZW depacker
 * (indexing table_four by -2).
 */

TEST(test_fuzzer_depack_lzx_invalid)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_lzx_invalid.lzx");
	fail_unless(ret == -XMP_ERROR_FORMAT, "depacking");

	xmp_free_context(opaque);
}
END_TEST
