#include "test.h"

/* This input caused uninitialized reads in the Digital Tracker loader
 * due to a missing instrument name hio_read return value check.
 */

TEST(test_fuzzer_dt_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dt_truncated.dtm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
