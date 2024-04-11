#include "test.h"

/* This input crashed the ProWizard TP2/3 depacker due to
 * a samples count >31.
 */

TEST(test_fuzzer_prowizard_tp3_samples_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_tp3_samples_bound");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
