#include "test.h"

/* This input crashed the ProWizard DI test function
 * due to reading a byte past the end of the file buffer.
 */

TEST(test_fuzzer_prowizard_di_patterns_test)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_di_patterns_test.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
