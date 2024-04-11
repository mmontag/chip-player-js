#include "test.h"

/* This input caused hangs in the GDM loader due to missing EOF checks
 * in the pattern scan and loading loops.
 */

TEST(test_fuzzer_gdm_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gdm_truncated.gdm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
