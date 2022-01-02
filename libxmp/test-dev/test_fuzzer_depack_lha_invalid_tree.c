#include "test.h"

/* This input caused hangs and large file output from the LHA depacker
 * due to encountering characters in the Huffman tree with 0-bit encodings.
 */

TEST(test_fuzzer_depack_lha_invalid_tree)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_lha_invalid_tree.lha");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
