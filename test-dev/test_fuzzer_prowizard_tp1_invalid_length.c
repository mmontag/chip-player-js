#include "test.h"

/* This input crashed the ProWizard TP1 depacker due to
 * a missing module length bounds check.
 */

TEST(test_fuzzer_prowizard_tp1_invalid_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_tp1_invalid_length.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
