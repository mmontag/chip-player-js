#include "test.h"

/* This input corrupted misc. module data due to a missing
 * sanity check on the number of channels.
 */

TEST(test_fuzzer_ult_channels_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ult_channels_bound.ult");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
