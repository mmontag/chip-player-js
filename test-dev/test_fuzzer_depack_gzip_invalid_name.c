#include "test.h"

/* This input caused hangs due to missing error checks in the
 * gzip depacker when skipping the name and comment fields.
 */

TEST(test_fuzzer_depack_gzip_invalid_name)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_gzip_invalid_name.gz");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
