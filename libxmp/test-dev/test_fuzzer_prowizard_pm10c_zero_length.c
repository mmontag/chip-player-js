#include "test.h"

/* This input caused uninitialized reads in the ProWizard loader due to
 * buggy pattern counting in the Promizer 1.0c loader causing it to emit
 * no patterns for a module with 0 orders.
 */

TEST(test_fuzzer_prowizard_pm10c_zero_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pm10c_zero_length");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
