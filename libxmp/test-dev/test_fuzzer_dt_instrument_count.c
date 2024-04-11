#include "test.h"

/* This input caused leaks in the Digital Tracker loader due to attempting
 * to load an invalid number of instruments.
 */

TEST(test_fuzzer_dt_instrument_count)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dt_instrument_count.dtm.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
