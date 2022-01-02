#include "test.h"

/* This input caused leaks in the RTM loader due to containing zero
 * samples and libxmp_realloc_samples ignoring m->xtra.
 */

TEST(test_fuzzer_rtm_zero_samples)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_rtm_zero_samples.rtm");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
