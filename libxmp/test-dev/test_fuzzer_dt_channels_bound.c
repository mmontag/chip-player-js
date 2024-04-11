#include "test.h"

/* This input caused hangs and massive RAM consumption in the Digital Tracker
 * loader due to a missing channel count bounds check.
 */

TEST(test_fuzzer_dt_channels_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dt_channels_bound.dtm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
