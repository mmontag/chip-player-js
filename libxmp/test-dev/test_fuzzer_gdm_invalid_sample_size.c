#include "test.h"

/* This input caused hangs and excessive memory consumption in the GDM
 * loader due to invalid sample sizes.
 */

TEST(test_fuzzer_gdm_invalid_sample_size)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gdm_invalid_sample_size.gdm");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
