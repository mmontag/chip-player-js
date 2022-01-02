#include "test.h"

/* This input crashed the ProWizard Pha depacker due to
 * missing bounding on the pattern count.
 */

TEST(test_fuzzer_prowizard_pha_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pha_patterns_bound");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
