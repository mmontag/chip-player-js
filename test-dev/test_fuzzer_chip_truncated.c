#include "test.h"

/* This input caused uninitialized reads in the Chiptracker loader
 * due to missing bounds checks when loading tracks.
 */

TEST(test_fuzzer_chip_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_chip_truncated.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
