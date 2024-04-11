#include "test.h"

/* This input crashed the ProWizard DI test function due to
 * bad PW_REQUEST_DATA usage.
 */

TEST(test_fuzzer_prowizard_di_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_di_truncated");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
