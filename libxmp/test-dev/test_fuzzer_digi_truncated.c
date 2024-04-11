#include "test.h"

/* These inputs caused uninitialized reads in the DIGI Booster loader
 * due to not checking the return value of hio_read.
 */

TEST(test_fuzzer_digi_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_digi_truncated.digi");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/load_digi_truncated2.digi");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/load_digi_truncated3.digi");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/load_digi_truncated4.digi");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
