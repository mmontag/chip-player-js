#include "test.h"

/* This input caused out-of-bounds reads in the MMD0/1 loader
 * due to having a higher channel count than the maximum channel
 * count in MMD0/1 modules.
 */

TEST(test_fuzzer_mmd1_channel_count)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mmd1_channel_count.med");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
