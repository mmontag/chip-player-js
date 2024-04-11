#include "test.h"

/* This input caused leaks due to containing an invalid number of
 * instruments.
 */

TEST(test_fuzzer_stx_instruments_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_stx_instruments_bound.stx");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
