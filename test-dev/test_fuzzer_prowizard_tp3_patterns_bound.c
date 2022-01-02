#include "test.h"

/* This input crashed the ProWizard TP2/3 depacker due to
 * a missing patterns bound check.
 */

TEST(test_fuzzer_prowizard_tp3_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_tp3_patterns_bound");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
