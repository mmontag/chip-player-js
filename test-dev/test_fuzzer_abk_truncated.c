#include "test.h"

/* This input caused an out-of-bounds read in the ABK test
 * function due to a missing hio_read return value check.
 */

TEST(test_fuzzer_abk_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_abk_truncated.abk");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
