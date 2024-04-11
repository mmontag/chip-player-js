#include "test.h"

/* This input crashed the ProWizard DI unpacker due to a
 * missing pattern count bound.
 */

TEST(test_fuzzer_prowizard_di_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_di_patterns_bound");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
