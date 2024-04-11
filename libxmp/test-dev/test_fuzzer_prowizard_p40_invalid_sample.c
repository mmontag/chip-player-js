#include "test.h"

/* These inputs caused undefined behavior in the ProWizard loader due to
 * missing sanity checks on the sample offset and sample loop offset.
 */

TEST(test_fuzzer_prowizard_p40_invalid_sample)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	ret = xmp_load_module(opaque, "data/f/prowizard_p40_invalid_sample");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p40_invalid_sample2");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
