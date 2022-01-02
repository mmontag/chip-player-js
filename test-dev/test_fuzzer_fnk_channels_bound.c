#include "test.h"

TEST(test_fuzzer_fnk_channels_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input crashed the FNK loader due to containing a channel
	 * count of 0.
	 */
	ret = xmp_load_module(opaque, "data/f/load_fnk_channels_bound.fnk");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	/* This input crashed the FNK loader due to the loader attempting to
	 * put pattern break bytes in a nonexistent 2nd channel.
	 */
	ret = xmp_load_module(opaque, "data/f/load_fnk_channels_bound_2.fnk");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (2)");

	xmp_free_context(opaque);
}
END_TEST
