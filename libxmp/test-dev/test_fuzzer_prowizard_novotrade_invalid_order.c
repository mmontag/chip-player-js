#include "test.h"

/* This input caused the ProWizard loader to load invalid patterns
 * due to the Novotrade depacker not validating the order list.
 */

TEST(test_fuzzer_prowizard_novotrade_invalid_order)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_novotrade_invalid_order.ntp");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
