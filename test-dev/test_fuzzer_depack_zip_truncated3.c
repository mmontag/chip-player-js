#include "test.h"

/* This input caused an infinite loop in the ZIP depacker.
 */

TEST(test_fuzzer_depack_zip_truncated3)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_zip_truncated3.zip");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
