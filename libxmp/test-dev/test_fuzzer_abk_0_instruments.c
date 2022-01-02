#include "test.h"

/* This input caused an out-of-bounds read in the
 * ABK loader due to containing 0 instruments.
 */

TEST(test_fuzzer_abk_0_instruments)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_abk_0_instruments.abk");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
