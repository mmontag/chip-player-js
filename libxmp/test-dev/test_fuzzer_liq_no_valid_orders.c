#include "test.h"

/* This input caused scan_module to attempt to endlessly scan
 * a pattern due to the module containing no valid orders.
 */

TEST(test_fuzzer_liq_no_valid_orders)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_liq_no_valid_orders.liq");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
