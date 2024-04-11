#include "test.h"

/* This input crashed the MED4 loader due to containing an
 * instrument name length byte >40.
 */

TEST(test_fuzzer_med4_instrument_name)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_med4_instrument_name.med");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
