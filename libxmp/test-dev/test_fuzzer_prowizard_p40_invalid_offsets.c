#include "test.h"

/* This input caused undefined behavior in the ProWizard loader due to
 * missing sanity checks on the pattern list/patterns/samples base offsets.
 */

TEST(test_fuzzer_prowizard_p40_invalid_offsets)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_p40_invalid_offsets");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
