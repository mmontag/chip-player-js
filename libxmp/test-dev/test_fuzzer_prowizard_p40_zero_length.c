#include "test.h"

/* This input caused uninitialized reads in the ProWizard loader due to
 * the input being a The Player 4.x "module" with 0 patterns.
 */

TEST(test_fuzzer_prowizard_p40_zero_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_p40_zero_length");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
