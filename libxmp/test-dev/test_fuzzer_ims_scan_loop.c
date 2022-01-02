#include "test.h"

/* This input caused scan_module to attempt to endlessly scan
 * a pattern due to an infinite jump loop between patterns combined
 * with scan_module expecting a pattern loop (E6x) to occur.
 */

TEST(test_fuzzer_ims_scan_loop)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ims_scan_loop.ims");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
