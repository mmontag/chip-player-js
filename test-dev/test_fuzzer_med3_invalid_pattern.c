#include "test.h"

/* This input caused out-of-bounds reads in the MED3 loader due
 * to faulty bounds checks when unpacking its invalid patterns.
 */

TEST(test_fuzzer_med3_invalid_pattern)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_med3_invalid_pattern.med");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
