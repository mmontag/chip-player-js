#include "test.h"

/* This input crashed undefined double to int conversions in the software
 * mixer due to having very invalid sample start and end values. It also
 * crashes Ultra Tracker.
 */

TEST(test_fuzzer_ult_invalid_sample)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ult_invalid_sample.ult");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
