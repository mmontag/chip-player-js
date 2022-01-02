#include "test.h"

/* This input caused UMRs in the XM loader due to the song length and
 * order list header size being different.
 */

TEST(test_fuzzer_xm_orders_mismatch)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_xm_orders_mismatch.xm");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
