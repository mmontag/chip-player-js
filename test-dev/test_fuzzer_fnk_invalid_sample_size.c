#include "test.h"

/* This input caused slow loading and excessive memory consumption in the
 * Funktracker loader due to invalid sample sizes.
 */

TEST(test_fuzzer_fnk_invalid_sample_size)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_fnk_invalid_sample_size.fnk.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
