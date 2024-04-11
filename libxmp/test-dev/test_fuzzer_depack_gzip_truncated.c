#include "test.h"

/* This input caused an out-of-bounds read in the inflate
 * algorithm due to indexing an array by the output of getc.
 */

TEST(test_fuzzer_depack_gzip_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_gzip_truncated.gz");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
