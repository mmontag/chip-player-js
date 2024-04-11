#include "test.h"

/* This input caused stack corruption in the ProWizard Novotrade
 * depacker due to an out of bounds pattern count.
 */

TEST(test_fuzzer_prowizard_novotrade_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_novotrade_patterns_bound.ntp");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
