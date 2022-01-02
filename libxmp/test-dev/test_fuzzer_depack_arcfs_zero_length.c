#include "test.h"

/* This input caused an out-of-bounds read in the ArcFS depacker
 * due to missing checks for 0-length files and no checks for
 * uncompressed file lengths.
 */

TEST(test_fuzzer_depack_arcfs_zero_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arcfs_zero_length");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
