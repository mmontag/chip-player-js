#include "test.h"

/* This input caused out-of-bound reads in the MDL loader
 * due to using a run event code as the first event in a track.
 */

TEST(test_fuzzer_mdl_invalid_run)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_run.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
