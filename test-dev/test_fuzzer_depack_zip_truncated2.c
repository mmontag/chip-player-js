#include "test.h"

/* This input caused a memory leak in inflate.c due to mishandling
 * of the fixed Huffman tree pointer.
 */

TEST(test_fuzzer_depack_zip_truncated2)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_zip_truncated2.zip");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
