#include "test.h"

/* This input caused high RAM usage in the MMD2/3 loader due to
 * a bad sanity check on the MMDInfo comment size.
 */

TEST(test_fuzzer_mmd3_invalid_mmdinfo)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mmd3_invalid_mmdinfo.med");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
