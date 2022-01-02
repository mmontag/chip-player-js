#include "test.h"

/* This input caused uninitialized reads in the SFX loader
 * due to a missing EOF check when loading patterns.
 */

TEST(test_fuzzer_sfx_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_sfx_truncated.sfx");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
