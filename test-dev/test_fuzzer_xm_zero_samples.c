#include "test.h"

/* This input caused leaks in the XM loader due to containing zero
 * samples and libxmp_realloc_samples ignoring m->xtra.
 */

TEST(test_fuzzer_xm_zero_samples)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_xm_zero_samples.xm");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
