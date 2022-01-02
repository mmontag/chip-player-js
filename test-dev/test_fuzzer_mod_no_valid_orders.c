#include "test.h"

/* This input caused scan_module to attempt to endlessly scan
 * a pattern due to the module containing no valid orders.
 */

TEST(test_fuzzer_mod_no_valid_orders)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mod_no_valid_orders.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
