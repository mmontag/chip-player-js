#include "test.h"

/* This input caused out of bounds reads in the Galaxy 5.0 loader
 * due to a missing channels count bound check.
 */

TEST(test_fuzzer_gal5_channels_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gal5_channels_bound");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
