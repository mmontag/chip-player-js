#include "test.h"

/* This input crashed the MTM loader due to the upper channel
 * bound of 32 (due to the panning array) not being enforced.
 */

TEST(test_fuzzer_mtm_channels_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mtm_channels_bound.mtm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
