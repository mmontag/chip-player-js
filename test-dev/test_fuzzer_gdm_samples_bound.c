#include "test.h"

/* This input caused leaks in the GDM loader due to loading an
 * invalid 256th instrument.
 */

TEST(test_fuzzer_gdm_samples_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gdm_samples_bound.gdm.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
